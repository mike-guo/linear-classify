////////////////////////////////////////////////////////
//
//   linear-classify.cpp    -- for Multi-Organ PlantNet
//
//   by Mike @ 2017.04.29
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <iomanip>
using namespace std;



void ExitWithHelp()
{
	printf( "Usage: linear-classify [options] data_set_file [predict_out_file]\n"
	        "options: \n"
	        "-t type : set algorithm type of calculating final features (default 0)\n"
	        "     0 -- WEIGHT \n"
	        "     1 -- MAX    \n"
	        "     2 -- AUTO: execute with a series of WEIGHT & MAX type and find best result\n"
	        "-r ratio : set ratio in WEIGHT type (default 1:1:1)\n"
	        "-m max_ratio : set max_ratio in AUTO type (default 5)\n" );
	exit(1);
}



// Definition of data struct
struct Data
{
	int label;
	int pred;
	double * feature[4] = { NULL, NULL, NULL, NULL };  // 0 - Final, 1 - Entire, 2 - Leaf, 3 - Flower
};

struct Data * DataSet = NULL;

size_t DataNum = 0;
size_t FeatureNum = 0;

enum ALGOTYPE { WEIGHT, MAX, AUTO };

#define    MAX_LINE_LENGTH    1024 * 16    // Support about 500 categories at most



void CleanBeforeExit()
{
	for (size_t i = 0; i < DataNum; i++)
	{
		if (DataSet[i].feature[0] != NULL)
		{
			delete[] DataSet[i].feature[0];
			DataSet[i].feature[0] = NULL;
		}
		if (DataSet[i].feature[1] != NULL)
		{
			delete[] DataSet[i].feature[1];
			DataSet[i].feature[1] = NULL;
		}
		if (DataSet[i].feature[2] != NULL)
		{
			delete[] DataSet[i].feature[2];
			DataSet[i].feature[2] = NULL;
		}
		if (DataSet[i].feature[3] != NULL)
		{
			delete[] DataSet[i].feature[3];
			DataSet[i].feature[3] = NULL;
		}
	}

	if (DataSet != NULL)
	{
		delete[] DataSet;
		DataSet = NULL;
	}
}

void ParseParams(int argc, char ** argv, int &type, int * w, int &max_ratio, char * input_file, char * output_file)
{
	// Parse options
	string ratio = "";
	int i;
	for (i = 1; i < argc; i++)  // 1st arg is "linear-classify"
	{
		if (argv[i][0] != '-')  break;
		if (++i >= argc)  // No input_file string
		{
			ExitWithHelp();
		}
		string option = argv[i - 1] + 1;
		if (option == "t")
		{
			string str = argv[i];
			if (str != "0" && str != "1" && str != "2")
			{
				printf("Error! Invalid type.\n");
				exit(1);
			}
			type = atoi(argv[i]);
		}
		else if (option == "r")
		{
			if (type != WEIGHT)
			{
				printf("Error! \"-r\" only available in WEIGHT type.\n");
				exit(1);
			}
			ratio = argv[i];
		}
		else if (option == "m")
		{
			if (type != AUTO)
			{
				printf("Error! \"-m\" only available in AUTO type.\n");
				exit(1);
			}
			max_ratio = atoi(argv[i]);
			if (max_ratio == 0)
			{
				printf("Error! Invalid max_ratio.\n");
				exit(1);
			}
		}
		else if (option == "help")
		{
			ExitWithHelp();
		}
		else
		{
			printf("Error! Invalid option \"%s\".\n", argv[i - 1]);
			printf("Use \"-help\" for help.\n");
			exit(1);
		}
	}

	// Parse ratio
	if (ratio != "")
	{
		if (ratio.find(':') == string::npos)
		{
			printf("Error! Invalid ratio format.\n");
			exit(1);
		}
		else
		{
			size_t j = 0;
			string tmp = "";
			if (ratio[ratio.length() - 1] == ':')
			{
				printf("Error! Invalid ratio format.\n");
				exit(1);
			}
			else
			{
				tmp = ratio + ":";  // For the last weight
			}
			size_t pos = tmp.find(':');
			while (pos > 0 && pos != string::npos)
			{
				w[j] = atoi(tmp.substr(0, pos).c_str());
				j++;
				tmp = tmp.substr(pos + 1);
				pos = tmp.find(':');
			}
			if (j != 3)
			{
				printf("Error! Invalid ratio format.\n");
				exit(1);
			}
		}
	}	

	// Determine filenames
	if (i >= argc)
	{
		ExitWithHelp();
	}
	strcpy(input_file, argv[i]);
	if (i < argc - 1)
	{
		strcpy(output_file, argv[i + 1]);
	}
	else
	{
		char * p = strrchr(input_file, '/');
		if (p != NULL)
			p++;
		else
			p = input_file;
		sprintf(output_file, "%s.out", p);
	}
}

void ReadData(char * file_name, char delim)
{
	ifstream inStream;
	inStream.open(file_name, ios::in);
	if (inStream.fail())
	{
		printf("Error! File %s doesn't exist.\n", file_name);
		exit(1);
	}

	printf("Readind data...\n");

	// 1st Loop: Find DataNum & FeatureNum
	char sz_line[MAX_LINE_LENGTH];
	string tmp;
	size_t LineNum = 0, PairNum = 0;
	while (inStream.getline(sz_line, MAX_LINE_LENGTH, '\n'))
	{
		LineNum++;

		tmp = sz_line;
		if (tmp[tmp.length() - 1] == delim)
		{
			tmp[tmp.length() - 1] = '\0';
		}
		size_t pos = tmp.find_last_of(delim);
		if (pos == string::npos)
		{
			printf("Error! Invalid data format in Line %d.\n", LineNum);
			inStream.close();
			exit(1);
		}

		string last = tmp.substr(pos + 1);
		pos = last.find(':');
		if (pos == string::npos)
		{
			printf("Error! Invalid data format in Line %d.\n", LineNum);
			inStream.close();
			exit(1);
		}
		if (last.substr(pos + 1).find(':') != string::npos)
		{
			printf("Error! Invalid data format in Line %d.\n", LineNum);
			inStream.close();
			exit(1);
		}
		size_t num = atoi(last.substr(0, pos).c_str());
		if (num > PairNum)
		{
			PairNum = num;
		}
	}
	inStream.clear();
	inStream.seekg(0, ios::beg);

	if (LineNum == 0)
	{
		printf("Error! No data found.\n");
		inStream.close();
		exit(1);
	}
	if (PairNum == 0)
	{
		printf("Error! No feature data found.\n");
		inStream.close();
		exit(1);
	}
	if (PairNum % 3 != 0)
	{
		printf("Error! Invalid feature nums.\n");
		inStream.close();
		exit(1);
	}

	// Initialize data
	DataNum = LineNum;
	FeatureNum = PairNum / 3;
	DataSet = new Data[DataNum];
	for (size_t i = 0; i < DataNum; i++)
	{
		DataSet[i].feature[0] = new double[FeatureNum];
		DataSet[i].feature[1] = new double[FeatureNum];
		DataSet[i].feature[2] = new double[FeatureNum];
		DataSet[i].feature[3] = new double[FeatureNum];

		DataSet[i].label = 0;
		DataSet[i].pred = 0;
		for (size_t j = 0; j < FeatureNum; j++)
		{
			DataSet[i].feature[0][j] = 0;
			DataSet[i].feature[1][j] = 0;
			DataSet[i].feature[2][j] = 0;
			DataSet[i].feature[3][j] = 0;
		}
	}

	// 2nd Loop: Fill in data
	size_t iLine = 0;
	while (inStream.getline(sz_line, MAX_LINE_LENGTH, '\n'))
	{
		iLine++;
		tmp = sz_line;
		size_t pos = tmp.find(delim);
		string first = tmp.substr(0, pos);
		if (first.find(':') != string::npos)
		{
			printf("Error! Invalid data format in Line %d.\n", iLine);
			inStream.close();
			CleanBeforeExit();
			exit(1);
		}
		DataSet[iLine - 1].label = atoi(first.c_str());
		tmp = tmp.substr(pos + 1);

		if (tmp[tmp.length() - 1] != delim)
		{
			tmp += delim;  // For the last field split
		}

		pos = tmp.find(delim);
		while (pos != string::npos)
		{
			string pair = tmp.substr(0, pos);
			size_t colon = pair.find(':');
			if (colon == string::npos)
			{
				printf("Error! Invalid data format in Line %d.\n", iLine);
				inStream.close();
				CleanBeforeExit();
				exit(1);
			}
			if (colon == 0)
			{
				printf("index Error! Invalid data format in Line %d.\n", iLine);
				inStream.close();
				CleanBeforeExit();
				exit(1);
			}
			if (colon == pair.length() - 1)
			{
				printf("value Error! Invalid data format in Line %d.\n", iLine);
				inStream.close();
				CleanBeforeExit();
				exit(1);
			}
			if (pair.substr(colon + 1).find(':') != string::npos)
			{
				printf("Error! Invalid data format in Line %d.\n", iLine);
				inStream.close();
				CleanBeforeExit();
				exit(1);
			}
			size_t index = atoi(pair.substr(0, colon).c_str());
			double value = atof(pair.substr(colon + 1).c_str());
			if (index > FeatureNum * 3)
			{
				printf("Error! Too many features in Line %d.\n", iLine);
				inStream.close();
				CleanBeforeExit();
				exit(1);
			}

			DataSet[iLine - 1].feature[(index - 1) / FeatureNum + 1][(index - 1) % FeatureNum] = value;
			
			tmp = tmp.substr(pos + 1);
			pos = tmp.find(delim);
		}
	}
	inStream.close();
}

size_t Classify(int type, int w1 = 0, int w2 = 0, int w3 = 0)
{
	double w_sum = w1 + w2 + w3;
	if (type == WEIGHT && w_sum == 0)
	{
		printf("Error! Invalid weights.\n");
		CleanBeforeExit();
		exit(1);
	}

	//printf("Classifying...\n");

	size_t RightNum = 0;
	for (size_t i = 0; i < DataNum; i++)
	{
		size_t max_index = 0;
		double max_value = 0;
		double sum_of_MAX = 0;  // For narmalization of MAX
		for (size_t j = 0; j < FeatureNum; j++)
		{
			double organ_value = 0;
			switch (type)
			{
			case WEIGHT:		
				DataSet[i].feature[0][j] = ( DataSet[i].feature[1][j] * w1 
										   + DataSet[i].feature[2][j] * w2 
										   + DataSet[i].feature[3][j] * w3 ) / w_sum;  // Normalize
				break;

			case MAX:
				for (size_t organ = 1; organ <= 3; organ++)
				{
					if (DataSet[i].feature[organ][j] > organ_value)
					{
						organ_value = DataSet[i].feature[organ][j];
					}
				}
				DataSet[i].feature[0][j] = organ_value;
				sum_of_MAX += organ_value;
				break;

			default:
				DataSet[i].feature[0][j] = 0;
				break;
			}

			if (DataSet[i].feature[0][j] > max_value)
			{
				max_index = j;
				max_value = DataSet[i].feature[0][j];
			}
		}
		DataSet[i].pred = max_index;  // Top 1
		if (DataSet[i].pred == DataSet[i].label)
		{
			RightNum++;
		}
		
		// Normalize of MAX
		if (type == MAX)
		{
			for (size_t j = 0; j < FeatureNum; j++)
			{				
				DataSet[i].feature[0][j] /= sum_of_MAX;
			}
		}
	}
	return RightNum;
}

void WriteFile(char * file_name)
{
	printf("Writting outfile %s...\n", file_name);
	
	ofstream outStream;
	outStream.open(file_name);
	for (size_t i = 0; i < DataNum; i++)
	{
		outStream << DataSet[i].label << " ";
		outStream << DataSet[i].pred << " ";
		outStream << setprecision(2);
		outStream << setiosflags(ios::fixed);
		for (size_t j = 0; j < FeatureNum; j++)
		{
			outStream << DataSet[i].feature[0][j] << " ";
		}
		outStream << endl;
	}
	outStream.close();
}



int gcd(int a, int b)  // Greatest Common Divisor
{
	if (a == 0 || b == 0)
	{
		return 0;
	}
	if (b > a)
	{
		int t = a;
		a = b, b = t;
	}
	while (a % b != 0)
	{
		int t = a % b;
		a = b, b = t;
	}
	return b;
}

int gcd(int a, int b, int c)
{
	if (a == 0 || b == 0 || c == 0)
	{
		return 0;
	}
	int t = gcd(a, b);
	return gcd(t, c);
}

int main(int argc, char ** argv)
{
	// Default Params
	int type = WEIGHT;
	int w[3] = { 1, 1, 1 };
	int max_ratio = 5;
	char input_file[1024] = "";
	char output_file[1024] = "";
	char delim = ' ';


	string str = "";
	str = "mike";



	ParseParams(argc, argv, type, w, max_ratio, input_file, output_file);


	ReadData(input_file, delim);


	printf("Classifying...\n");
	if (type == AUTO)
	{
		size_t loop_num = 0;
		size_t max_crtnum = 0;

		// AUTO WEIGHT
		for (int w1 = 0; w1 <= max_ratio; w1++)
		for (int w2 = 0; w2 <= max_ratio; w2++)
		for (int w3 = 0; w3 <= max_ratio; w3++)
		{
			if (w1 == 0 && w2 == 0 && w3 == 0)  continue;

			if (w1 == 0 && w2 == 0 && w3 != 1)  continue;
			if (w1 == 0 && w3 == 0 && w2 != 1)  continue;
			if (w2 == 0 && w3 == 0 && w1 != 1)  continue;

			if (w1 == 0 && w2 != 0 && w3 != 0 && gcd(w2, w3) != 1)  continue;
			if (w2 == 0 && w1 != 0 && w3 != 0 && gcd(w1, w3) != 1)  continue;
			if (w3 == 0 && w1 != 0 && w2 != 0 && gcd(w1, w2) != 1)  continue;

			if (w1 != 0 && w2 != 0 && w3 != 0 && gcd(w1, w2, w3) != 1)  continue;

			size_t crtnum = Classify(WEIGHT, w1, w2, w3);
			printf("accuracy = %.4f%%, ratio = %d:%d:%d\n", (double) crtnum / DataNum * 100, w1, w2, w3);
			loop_num++;
			if (crtnum > max_crtnum)
			{
				max_crtnum = crtnum;
				type = WEIGHT;
				w[0] = w1; w[1] = w2; w[2] = w3;
			}
		}
		
		// AUTO MAX
		size_t crtnum = Classify(MAX);
		printf("accuracy = %.4f%%, type = MAX\n", (double) crtnum / DataNum * 100);
		loop_num++;
		if (crtnum > max_crtnum)
		{
			max_crtnum = crtnum;
			type = MAX;
		}

		printf("loop_num = %d\n", loop_num);
	}
	size_t crt_num = Classify(type, w[0], w[1], w[2]);


	WriteFile(output_file);
	

	printf("==========================================\n");
	switch (type)
	{
	case WEIGHT:
		printf("    AlgoType: WEIGHT - %d:%d:%d\n", w[0], w[1], w[2]);
		break;
	case MAX:
		printf("    AlgoType: MAX\n");
		break;
	default:
		break;
	}
	printf("    Accuracy: %.4f%% ( %d / %d )\n", (double) crt_num / DataNum * 100, crt_num, DataNum);
	printf("==========================================\n");

	
	CleanBeforeExit();
	return 0;
}
