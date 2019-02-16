#include "Python.h"
#include "FastPythonGcodeParser.h"
#include "GcodeParser.h"
#include <iostream>


int main(int argc, char *argv[])
{
	Py_SetProgramName(argv[0]);
	initfastgcodeparser();
	return 0;
}

static PyMethodDef CppGcodeParserMethods[] = {
	{ "ParseGcode",  (PyCFunction)ParseGcode,  METH_VARARGS  ,"Parses Gcode." },
	{ NULL, NULL, 0, NULL }
};

extern "C" void  initfastgcodeparser(void)
{
    std::cout << "Initializing Octolapse Fast Gcode Parser V1.0.\r\n";
    Py_Initialize();
	PyObject *m;
    text_only_functions.clear();
    for( unsigned int a = 0; a < sizeof(text_only_function_names)/sizeof(text_only_function_names[0]); a = a + 1 )
    {
        text_only_functions.insert(text_only_function_names[a]);
    }
    text_only_functions.clear();

    for( unsigned int a = 0; a < sizeof(parsable_command_names)/sizeof(parsable_command_names[0]); a = a + 1 )
    {
        parsable_commands.insert(parsable_command_names[a]);
    }

	m = Py_InitModule("fastgcodeparser", CppGcodeParserMethods);
	moduleError = PyErr_NewException("fastgcodeparser.error", NULL, NULL);
	Py_INCREF(moduleError);
	PyModule_AddObject(m, "error", moduleError);
}

extern "C"  PyObject* ParseGcode(PyObject* self, PyObject *args)
{
    char * gcodeParam;
	if (!PyArg_ParseTuple(args, "s", &gcodeParam))
	{
		PyErr_SetString(moduleError, "Parse Gcode requires at least one parameter: the gcode string");
		return NULL;
	}

    std::string strippedCommand = stripGcode(gcodeParam);
    if (strippedCommand.size() == 0 || !isGcodeWord(strippedCommand[0]))
    {
		return Py_BuildValue("O", Py_False);
    }
    std::string commandName = "";
    commandName.append(strippedCommand,0,1);
    PyObject * pyCommandName;
    PyObject * pyParametersDict;
    bool hasParameters = false;

    int endAddressIndex;
    if (commandName == "T")
    {
        endAddressIndex=0;
    }
    else
    {
        endAddressIndex = getFloatEndindex(strippedCommand, 1);
        if (endAddressIndex > 1)
        {
            commandName.append(strippedCommand,1, endAddressIndex-1);
        }
    }

    if (parsable_commands.find(commandName) == parsable_commands.end())
    {
        pyParametersDict = PyDict_New();
        PyObject *falseValue = Py_BuildValue("O", Py_False);
        PyDict_SetItemString(pyParametersDict, "parameters", falseValue);
        Py_DECREF(falseValue);
        hasParameters=true;
    }
	else if ( strippedCommand.size() > endAddressIndex)
	{
	    if (text_only_functions.find(commandName) != text_only_functions.end())
	    {
	        pyParametersDict = getTextOnlyParameter(commandName, gcodeParam);
	    }
        else
        {
            pyParametersDict = getParameters(strippedCommand, endAddressIndex);

        }
        hasParameters=true;

	}

    pyCommandName = PyString_FromString(commandName.c_str());
    PyObject *ret_val;
    if (!hasParameters)
    {
        ret_val = PyTuple_Pack(2,pyCommandName,Py_None);
    }
    else
    {
        ret_val = PyTuple_Pack(2,pyCommandName,pyParametersDict);
        Py_DECREF(pyParametersDict);
    }
    Py_DECREF(pyCommandName);

	return ret_val;

}

static PyObject* getTextOnlyParameter(std::string commandName, std::string gcodeParam)
{
    PyObject *parameterDict = PyDict_New();
    int textIndex;
    int foundCount = 0;
    int textStartIndex = -1;
    bool skippedSpace = false;
    for(textIndex=0 ; textIndex < gcodeParam.size(); textIndex++ )
    {
        if (textStartIndex < 0)
        {
            if(gcodeParam[textIndex] == commandName[foundCount])
            {
                foundCount++;
                if (foundCount == commandName.size())
                {
                    textStartIndex = textIndex+1;
                    continue;
                }
            }
        }
        else if (!skippedSpace )
        {
            if( gcodeParam[textIndex]== ' ')
                textStartIndex++;
            skippedSpace = true;
        }
        else if(gcodeParam[textIndex] == ';' || gcodeParam[textIndex] == '\r' || gcodeParam[textIndex] == '\n')
        {
            break;
        }

    }
    std::string text="";
    if (textIndex - textStartIndex > 0)
    {
        text = gcodeParam.substr(textStartIndex,textIndex - textStartIndex);
    }
    PyObject * parameter = PyString_FromString(text.c_str());
    PyDict_SetItemString(parameterDict, "TEXT", parameter); // reference to num stolen
    Py_DECREF(parameter);
    return parameterDict;


}
static PyObject* getParameters(std::string commandString, int startIndex)
{
    PyObject *parameterDict = PyDict_New();
	getParameters(commandString, startIndex, parameterDict);
	return parameterDict;
}

static void getParameters(std::string commandString, int startIndex, PyObject * parameterDict)
{

    std::string parameterName = "";
    parameterName.append(commandString, startIndex, 1);
	if (startIndex < commandString.size())
	{
		int endParameterIndex = 2;

		bool isTCommand = startIndex == 0 && commandString[0]=='T';
		if (!isTCommand)
        {
		    endParameterIndex = getFloatEndindex(commandString, startIndex + 1);
        }

		PyObject *valString = NULL;
		PyObject * val = NULL;
		if (endParameterIndex < 0 || endParameterIndex - (startIndex+1) < 1)
		{
		    PyDict_SetItemString(parameterDict, parameterName.c_str(), Py_None); // reference to num stolen
		}
		else
		{
		    std::string value="";
		    value.append(commandString, startIndex+1, endParameterIndex - startIndex-1);

			char ** pend = NULL;
			if(!isTCommand)
			{
                valString = PyString_FromString(value.c_str());
                if (valString == NULL)
                {
                    return;
                }
                val = PyFloat_FromString(valString, pend);
                if (val == NULL)
                {
                    val = valString;
                    valString = NULL;
                }

            }
            else
            {
                char * value_cstr = new char[value.size()+1];
                strcpy(value_cstr, value.c_str());
                val = PyInt_FromString(value_cstr,pend,0);
                if(val == NULL)
                {
                    PyErr_Clear();
                    val = PyString_FromString(value.c_str());
                    if (val == NULL)
                    {
                        return;
                    }
                }
            }

			if (val == NULL)
			    val = valString;
            PyDict_SetItemString(parameterDict, parameterName.c_str(), val); // reference to num stolen
            if(valString!= NULL)
                Py_DECREF(valString);
            if(val != NULL)
                Py_DECREF(val);
		}
		if (endParameterIndex < commandString.size())
		{
		    getParameters(commandString, endParameterIndex, parameterDict);
        }
	}
}
