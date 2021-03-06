#include "stdafx.h"
#include <stdio.h>
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#define D_SCL_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#pragma warning(disable : 4996)

#include <CL/cl.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitmap_image.hpp"
#include <math.h>
#include <list>
//#include <unistd.h>

#define NUM_DATA 100
#ifdef Open_MP
printf("Yes");
#endif

float** getGaussFilter(int size, float teta, float sigma) {
	float sigma2d = sigma * sigma;
	float **res = new float*[size];
	float cosT = cos(teta);
	for (int x = -floor(size / 2); x <= floor(size / 2);x++) {
		res[x] = new float[size];
		for (int y = -floor(size / 2); y <= floor(size / 2);y++) {
			float k = pow(-x * cosT + y * cosT, 2) / sigma2d / (sigma2d - 1);
			res[x][y] = k * exp(-(pow(x*cosT + y * cosT, 2) + pow(-x * cosT + y * cosT, 2)) / 2 / sigma2d);
		}
	}
	return res;
}

const char** readKernelProgram(std::string filename) {
	std::ifstream f(filename);
	string str;

	const char **program = new const char*[100];
	int i = 0;
	while (getline(f, str)) {
		program[i] = str.c_str();
		cout << str.c_str() << endl;
	}

	/*list<string> program;
	while (std::getline(f, str)) {
	program.push_front(str);
	cout << str << endl;
	}
	f.close();

	const char **programChr = new const char*[program.size()];
	int i = 0;
	for (list<string>::iterator it = program.begin(); it != program.end(); ++it) {
	programChr[i] = (*it).data(); // new char[(*it).length()];
	}*/

	return program;
}

void ShowInfo() {
	cl_int err;
	cl_uint numPlatforms;
	cl_platform_id platforms[5];

	err = clGetPlatformIDs(5, platforms, &numPlatforms);
	if (CL_SUCCESS == err)
		printf("\nDetected OpenCL platforms: %d", numPlatforms);
	else
		printf("\nError calling clGetPlatformIDs. Error code: %d", err);

	for (int i = 0; i<numPlatforms; i++)
	{
		char buffer[10240];
		printf("  -- %d --\n", i);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, 10240, buffer, NULL);
		printf("  PROFILE = %s\n", buffer);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, 10240, buffer, NULL);
		printf("  VERSION = %s\n", buffer);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 10240, buffer, NULL);
		printf("  NAME = %s\n", buffer);
		clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, 10240, buffer, NULL);
		printf("  VENDOR = %s\n", buffer);
		//clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, 10240, buffer, NULL);
		//printf("  EXTENSIONS = %s\n", buffer);
	}

	cl_device_id devices[100];
	cl_uint devices_n = 0;
	clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 100, devices, &devices_n);

	printf("=== %d OpenCL device(s) found on platform:\n", devices_n);
	for (int i = 0; i<devices_n; i++)
	{
		char buffer[10240];
		cl_uint buf_uint;
		cl_ulong buf_ulong;
		printf("  -- %d --\n", i);
		clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(buffer), buffer, NULL);
		printf("  DEVICE_NAME = %s\n", buffer);
		clGetDeviceInfo(devices[i], CL_DEVICE_VENDOR, sizeof(buffer), buffer, NULL);
		printf("  DEVICE_VENDOR = %s\n", buffer);
		clGetDeviceInfo(devices[i], CL_DEVICE_VERSION, sizeof(buffer), buffer, NULL);
		printf("  DEVICE_VERSION = %s\n", buffer);
		clGetDeviceInfo(devices[i], CL_DRIVER_VERSION, sizeof(buffer), buffer, NULL);
		printf("  DRIVER_VERSION = %s\n", buffer);
		clGetDeviceInfo(devices[i], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(buf_uint), &buf_uint, NULL);
		printf("  DEVICE_MAX_COMPUTE_UNITS = %u\n", (unsigned int)buf_uint);
		clGetDeviceInfo(devices[i], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(buf_uint), &buf_uint, NULL);
		printf("  DEVICE_MAX_CLOCK_FREQUENCY = %u\n", (unsigned int)buf_uint);
		clGetDeviceInfo(devices[i], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(buf_ulong), &buf_ulong, NULL);
		printf("  DEVICE_GLOBAL_MEM_SIZE = %llu\n", (unsigned long long)buf_ulong);
	}

}

int* getIntensityMatrix(bitmap_image image) {
	unsigned int imageHeight = image.height();
	unsigned int imageWidth = image.width();

	unsigned char red, green, blue;
	int *intensityMatrix = new int[imageHeight * imageWidth];

	for (int i = 0; i<imageHeight; i++)
		for (int j = 0; j<imageWidth; j++)
		{
			image.get_pixel(j, i, red, green, blue);
			intensityMatrix[i * imageWidth + j] = (red + green + blue) / 3;
		}
	return intensityMatrix;
}

int main(void)
{
	ShowInfo();
	std::string filename("orchid_flower.bmp");
	bitmap_image image(filename);
	unsigned int imageHeight = image.height();
	unsigned int imageWidth = image.width();
	int imageSize = imageHeight * imageWidth;

	int* intensityMatrix = getIntensityMatrix(image);


	float sigma[] = { 1.2, 1.5, 1.7, 2 };

	float teta = CL_M_PI * 0.25;
	float** filter1 = getGaussFilter(7, teta, sigma[0]);
	float** filter2 = getGaussFilter(7, teta, sigma[1]);
	float** filter3 = getGaussFilter(7, teta, sigma[2]);
	float** filter4 = getGaussFilter(7, teta, sigma[3]);

	cl_int err;
	cl_uint numPlatforms;
	cl_platform_id platforms[5];

	err = clGetPlatformIDs(5, platforms, &numPlatforms);

	const char **program_source = readKernelProgram("gpu_code.txt");
	/*const char *program_source[] = { "__kernel void filter_image(__global float *image, __global float **filter, __global float** outImage, __global int filter_size) {",
	"x = get_global_id(0);",
	"y = get_global_id(1);",
	"res = 0;",
	"for (int i = -floor(filter_size / 2); i<floor(filter_size / 2); i++) {",
	"for (int j = -floor(filter_size / 2); j<floor(filter_size / 2); j++) {",
	"res += image[x + floor(filter_size / 2) + i][y + floor(filter_size / 2) + j];",
	"}",
	"}",
	"outImage[x][y] = res;",
	"}" };*/


	cl_device_id devices[100];
	cl_uint devices_n = 0;
	clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 100, devices, &devices_n);


	//cl_int err;
	cl_context context;
	context = clCreateContext(NULL, 1, devices, NULL, NULL, &err);

	cl_program program;
	program = clCreateProgramWithSource(context, sizeof(program_source) / sizeof(*program_source), program_source, NULL, &err);
	if (clBuildProgram(program, 1, devices, "", NULL, NULL) != CL_SUCCESS) {
		char buffer[1024000];
		clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, NULL);
		fprintf(stderr, "CL Compilation failed:\n%s", buffer);
		abort();
	}
	clUnloadCompiler();

	cl_mem input_buffer;
	input_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(int)*NUM_DATA, NULL, &err);

	cl_mem output_buffer;
	output_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int)*NUM_DATA, NULL, &err);

	int factor = 2;

	cl_kernel kernel;
	kernel = clCreateKernel(program, "filter_image", &err);
	clSetKernelArg(kernel, 0, sizeof(input_buffer), &input_buffer);
	clSetKernelArg(kernel, 1, sizeof(output_buffer), &output_buffer);
	clSetKernelArg(kernel, 2, sizeof(factor), &factor);

	cl_command_queue queue;
	queue = clCreateCommandQueue(context, devices[0], 0, &err);

	for (int i = 0; i<NUM_DATA; i++) {
		clEnqueueWriteBuffer(queue, input_buffer, CL_TRUE, i * sizeof(int), sizeof(int), &i, 0, NULL, NULL);
	}

	cl_event kernel_completion;
	size_t global_work_size[1] = { NUM_DATA };
	clEnqueueNDRangeKernel(queue, kernel, 1, NULL, global_work_size, NULL, 0, NULL, &kernel_completion);
	clWaitForEvents(1, &kernel_completion);
	clReleaseEvent(kernel_completion);

	printf("Result:");
	for (int i = 0; i<NUM_DATA; i++) {
		int data;
		clEnqueueReadBuffer(queue, output_buffer, CL_TRUE, i * sizeof(int), sizeof(int), &data, 0, NULL, NULL);
		printf(" %d", data);
	}
	printf("\n");

	clReleaseMemObject(input_buffer);
	clReleaseMemObject(output_buffer);
	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseContext(context);

	system("pause");
	return 0;
}