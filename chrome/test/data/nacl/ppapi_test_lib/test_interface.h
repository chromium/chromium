// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Functions and constants for test registration and setup.
//
// NOTE: These must be implemented by the tester:
// - SetupTests()
// - SetupPluginInterfaces()
//
// Sample Usage:
//
//   void MyCallback(void* user_data, int32_t result) { ... }
//
//   void TestPPBFoo() {
//     // sync test case
//     PP_Resource my_resource = PPBFoo()->Create(kInvalidInstance);
//     EXPECT(my_resource == kInvalidResource);
//
//     // async test case
//     PP_CompletionCallback testable_callback =
//         MakeTestableCompletionCallback("MyCallback", MyCallback, NULL);
//     int32_t pp_error = PPBFoo()->AsyncFunction(testable_callback);
//     EXPECT(pp_error == PP_OK_COMPLETIONPENDING);
//
//     TEST_PASSED;
//   }
//
//   void SetupTests() {
//     RegisterTest("TestPPBFoo", TestPPBFoo);
//   }
//
//   const PPP_Bar ppp_bar_interface = { ... };
//
//   void SetupPluginInterface() {
//     RegisterPluginInterface(PPP_BAR_INTERFACE, &ppp_bar_interface);
//   }
//

#ifndef CHROME_TEST_DATA_NACL_PPAPI_TEST_LIB_TEST_INTERFACE_H_
#define CHROME_TEST_DATA_NACL_PPAPI_TEST_LIB_TEST_INTERFACE_H_

#include <stdint.h>
#include <stdio.h>
#include <limits>

#include <sstream>

#include "native_client/src/include/nacl_string.h"

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_image_data.h"

struct PP_Rect;
struct PP_Size;

////////////////////////////////////////////////////////////////////////////////
// These must be implemented by the tester
////////////////////////////////////////////////////////////////////////////////

// Use RegisterTest() to register each TestFunction.
void SetupTests();
// Use RegisterPluginInterface() to register custom PPP_ interfaces other than
// PPP_Instance that is required and provided by default.
void SetupPluginInterfaces();

////////////////////////////////////////////////////////////////////////////////
// Test helpers
////////////////////////////////////////////////////////////////////////////////

// Registers test_function, so it is callable from JS using
// plugin.postMessage(test_name);
typedef void (*TestFunction)();
void RegisterTest(nacl::string test_name, TestFunction test_function);

// Registers ppp_interface, so it is returned by PPP_GetInterface().
void RegisterPluginInterface(const char* interface_name,
                             const void* ppp_interface);

// Helper for creating user callbacks whose invocation will be reported to JS.
// Callback setting allows for synchronous completion to make it easier to
// test error conditions.
// WARNING: Do not reuse this callback if the operation that took it as an arg
// returned PP_OK_COMPLETIONPENDING. The wrapper allocates data on creation
// and then deallocates it when the callback is invoked.
PP_CompletionCallback MakeTestableCompletionCallback(
    const char* callback_name,  // will be postmessage'ed to JS
    PP_CompletionCallback_Func func,
    void* user_data);
PP_CompletionCallback MakeTestableCompletionCallback(
    const char* callback_name,  // will be postmessage'ed to JS
    PP_CompletionCallback_Func func);

// Uses PPB_Messaging interface to post "test_name:message".
void PostTestMessage(nacl::string test_name, nacl::string message);

// Make a STRING var.
PP_Var PP_MakeString(const char* s);

// Convert var into printable string (for debuggin)
nacl::string StringifyVar(const PP_Var& var);

// Use to verify the result of a test and report failures.
#define EXPECT(expr) do { \
  if (!(expr)) { \
    char error[1024]; \
    snprintf(error, sizeof(error), \
             "ERROR at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    fprintf(stderr, "%s", error); \
    PostTestMessage(__FUNCTION__, error); \
  } \
} while (0)

// Check expected value of INT32 var.
#define EXPECT_VAR_INT(var, val) \
  EXPECT(var.type == PP_VARTYPE_INT32 && var.value.as_int == val)

// Check expected value of STRING var (val is 'char*')
#define EXPECT_VAR_STRING(var, val) \
  do { \
    EXPECT(var.type == PP_VARTYPE_STRING); \
    uint32_t dummy_size; \
    const char* expected = PPBVar()->VarToUtf8(var, &dummy_size); \
    EXPECT(0 == strcmp(expected, val)); \
  } while (0)

// Check expected value of BOOL var.
#define EXPECT_VAR_BOOL(var, val) \
  EXPECT(var.type == PP_VARTYPE_BOOL && var.value.as_bool == val)

// Use to report success.
#define TEST_PASSED PostTestMessage(__FUNCTION__, "PASSED");
// Or failure.
#define TEST_FAILED EXPECT(false)

// Handy for use with LOG_TO_BROWSER() convert arbitrary objects into strings.
template<typename T> nacl::string toString(T v) {
  std::stringstream s;
  s << v;
  return s.str();
}

// Log message for debugging or progress reporting purposes.
// If you use this with  nacltest.js::expectMessageSequence
// it will not interfere with output used for correctness checking.
#define LOG_TO_BROWSER(message) PostTestMessage("@", message)

// Cause a crash in a way that is guaranteed not to get optimized out by LLVM.
#define CRASH *(volatile int *) 0 = 0;

// Use this constant for stress testing
// (i.e. creating and using a large number of resources).
const int kManyResources = 1000;

////////////////////////////////////////////////////////////////////////////////
// PPAPI Helpers
////////////////////////////////////////////////////////////////////////////////

const PP_Instance kInvalidInstance = 0;
const PP_Module kInvalidModule = 0;
const PP_Resource kInvalidResource = 0;

// These should not exist.
// Chrome uses the bottom 2 bits to differentiate between different id types.
// 00 - module, 01 - instance, 10 - resource, 11 - var.
const PP_Instance kNotAnInstance = 0xFFFFF0;
const PP_Resource kNotAResource = 0xAAAAA0;

const PP_Point kOrigin = PP_MakePoint(0, 0);

// Interface pointers and ids corresponding to this plugin;
// set at initialization/creation.
PP_Instance pp_instance();
PP_Module pp_module();

// If you are providing your own version of PPP_Instance::DidCreate
// call this function to ensure proper test set-up.
PP_Bool DidCreateDefault(PP_Instance instance,
                         uint32_t argc, const char* argn[], const char* argv[]);
// Other default implementations of the required PPP_Instance functions.
void DidDestroyDefault(PP_Instance instance);
void DidChangeViewDefault(PP_Instance instance, PP_Resource view);
void DidChangeFocusDefault(PP_Instance instance, PP_Bool has_focus);
PP_Bool HandleDocumentLoadDefault(PP_Instance instance, PP_Resource url_loader);


bool IsSizeInRange(PP_Size size, PP_Size min_size, PP_Size max_size);
bool IsSizeEqual(PP_Size size, PP_Size expected);
bool IsRectEqual(PP_Rect position, PP_Rect expected);

// TODO(polina, nfullagar): allow specification of non-premultipled colors
// and provide alpha premultiplcation in FormatColor(). This will be required
// when future PPAPI pixel formats are extended to include non-premultipled
// or ignored alpha.

struct ColorPremul { uint32_t A, R, G, B; };  // Use premultipled Alpha.
const ColorPremul kSheerRed = { 0x88, 0x88, 0x00, 0x00 };
const ColorPremul kSheerBlue = { 0x88, 0x00, 0x00, 0x88 };
const ColorPremul kSheerGray = { 0x77, 0x55, 0x55, 0x55 };
const ColorPremul kOpaqueGreen = { 0xFF, 0x00, 0xFF, 0x00 };
const ColorPremul kOpaqueBlack = { 0xFF, 0x00, 0x00, 0x00 };
const ColorPremul kOpaqueWhite = { 0xFF, 0xFF, 0xFF, 0xFF };
const ColorPremul kOpaqueYellow = { 0xFF, 0xFF, 0xFF, 0x00 };
const int kBytesPerPixel = sizeof(uint32_t);  // 4 bytes for BGRA or RGBA.

// Assumes premultipled Alpha.
uint32_t FormatColor(PP_ImageDataFormat format, ColorPremul color);

// Creates image data resource and bitmap for a rectangular region of |size|
// and |pixel_color|.
PP_Resource CreateImageData(PP_Size size, ColorPremul pixel_color, void** bmp);


// Checks if the image rect of |color| and |size| is on the screen at |origin|.
bool IsImageRectOnScreen(PP_Resource graphics2d,
                         PP_Point origin,
                         PP_Size size,
                         ColorPremul color);

#endif  // CHROME_TEST_DATA_NACL_PPAPI_TEST_LIB_TEST_INTERFACE_H_
