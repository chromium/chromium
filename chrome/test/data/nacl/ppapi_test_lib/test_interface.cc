// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"

#include <string.h>
#include <map>
#include <new>

#include "chrome/test/data/nacl/ppapi_test_lib/get_browser_interface.h"
#include "chrome/test/data/nacl/ppapi_test_lib/internal_utils.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/private/ppb_testing_private.h"

void PostTestMessage(nacl::string test_name, nacl::string message) {
  nacl::string test_message = test_name;
  test_message += ":";
  test_message += message;
  PP_Var post_var = PPBVar()->VarFromUtf8(test_message.c_str(),
                                          test_message.size());
  PPBMessaging()->PostMessage(pp_instance(), post_var);
  PPBVar()->Release(post_var);
}

PP_Var PP_MakeString(const char* s) {
  return PPBVar()->VarFromUtf8(s, strlen(s));
}

nacl::string StringifyVar(const PP_Var& var) {
  uint32_t dummy_size;
  switch (var.type) {
    default:
     return "<UNKNOWN>" +  toString(var.type);
    case  PP_VARTYPE_NULL:
      return "<NULL>";
    case  PP_VARTYPE_BOOL:
     return "<BOOL>" + toString(var.value.as_bool);
    case  PP_VARTYPE_INT32:
     return "<INT32>" + toString(var.value.as_int);
    case  PP_VARTYPE_DOUBLE:
     return "<DOUBLE>" + toString(var.value.as_double);
    case PP_VARTYPE_STRING:
     return "<STRING>" + nacl::string(PPBVar()->VarToUtf8(var, &dummy_size));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Test registration
////////////////////////////////////////////////////////////////////////////////

namespace {

class TestTable {
 public:
  // Return singleton intsance.
  static TestTable* Get() {
    static TestTable table;
    return &table;
  }

  void AddTest(nacl::string test_name, TestFunction test_function) {
    test_map_[test_name] = test_function;
  }
  void RunTest(nacl::string test_name);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(TestTable);

  TestTable() {}

  typedef std::map<nacl::string, TestFunction> TestMap;
  TestMap test_map_;
};

void TestTable::RunTest(nacl::string test_name) {
  TestMap::iterator it = test_map_.find(test_name);
  if (it == test_map_.end()) {
    PostTestMessage(test_name, "NOTFOUND");
    return;
  }
  CHECK(it->second != NULL);
  TestFunction test_function = it->second;
  return test_function();
}

}  // namespace

void RegisterTest(nacl::string test_name, TestFunction test_func) {
  TestTable::Get()->AddTest(test_name, test_func);
}

void RunTest(nacl::string test_name) {
  TestTable::Get()->RunTest(test_name);
}

////////////////////////////////////////////////////////////////////////////////
// Testable callback support
////////////////////////////////////////////////////////////////////////////////

namespace {

struct CallbackInfo {
  nacl::string callback_name;
  PP_CompletionCallback user_callback;
};

void ReportCallbackInvocationToJS(const char* callback_name) {
  PP_Var callback_var = PPBVar()->VarFromUtf8(callback_name,
                                              strlen(callback_name));
  // Report using postmessage for async tests.
  PPBMessaging()->PostMessage(pp_instance(), callback_var);
  PPBVar()->Release(callback_var);
}

void CallbackWrapper(void* user_data, int32_t result) {
  CallbackInfo* callback_info = reinterpret_cast<CallbackInfo*>(user_data);
  PP_RunCompletionCallback(&callback_info->user_callback, result);
  ReportCallbackInvocationToJS(callback_info->callback_name.c_str());
  delete callback_info;
}

}  // namespace

PP_CompletionCallback MakeTestableCompletionCallback(
    const char* callback_name,  // Tested for by JS harness.
    PP_CompletionCallback_Func func,
    void* user_data) {
  CHECK(callback_name != NULL && strlen(callback_name) > 0);
  CHECK(func != NULL);

  CallbackInfo* callback_info = new(std::nothrow) CallbackInfo;
  CHECK(callback_info != NULL);
  callback_info->callback_name = callback_name;
  callback_info->user_callback =
    PP_MakeOptionalCompletionCallback(func, user_data);

  return PP_MakeOptionalCompletionCallback(CallbackWrapper, callback_info);
}

PP_CompletionCallback MakeTestableCompletionCallback(
    const char* callback_name,  // Tested for by JS harness.
    PP_CompletionCallback_Func func) {
  return MakeTestableCompletionCallback(callback_name, func, NULL);
}


////////////////////////////////////////////////////////////////////////////////
// PPAPI Helpers
////////////////////////////////////////////////////////////////////////////////

bool IsSizeInRange(PP_Size size, PP_Size min_size, PP_Size max_size) {
  return (min_size.width <= size.width && size.width <= max_size.width &&
          min_size.height <= size.height && size.height <= max_size.height);
}

bool IsSizeEqual(PP_Size size, PP_Size expected) {
  return (size.width == expected.width && size.height == expected.height);
}

bool IsRectEqual(PP_Rect position, PP_Rect expected) {
  return (position.point.x == expected.point.x &&
          position.point.y == expected.point.y &&
          IsSizeEqual(position.size, expected.size));
}

uint32_t FormatColor(PP_ImageDataFormat format, ColorPremul color) {
  if (format == PP_IMAGEDATAFORMAT_BGRA_PREMUL)
    return (color.A << 24) | (color.R << 16) | (color.G << 8) | (color.B);
  else if (format == PP_IMAGEDATAFORMAT_RGBA_PREMUL)
    return (color.A << 24) | (color.B << 16) | (color.G << 8) | (color.R);
  else
    NACL_NOTREACHED();
}

PP_Resource CreateImageData(PP_Size size, ColorPremul pixel_color, void** bmp) {
  PP_ImageDataFormat image_format = PPBImageData()->GetNativeImageDataFormat();
  uint32_t formatted_pixel_color = FormatColor(image_format, pixel_color);
  PP_Resource image_data = PPBImageData()->Create(
      pp_instance(), image_format, &size, PP_TRUE /*init_to_zero*/);
  CHECK(image_data != kInvalidResource);
  PP_ImageDataDesc image_desc;
  CHECK(PPBImageData()->Describe(image_data, &image_desc) == PP_TRUE);
  *bmp = NULL;
  *bmp = PPBImageData()->Map(image_data);
  CHECK(*bmp != NULL);
  uint32_t* bmp_words = static_cast<uint32_t*>(*bmp);
  int num_pixels = image_desc.stride / kBytesPerPixel * image_desc.size.height;
  for (int i = 0; i < num_pixels; i++)
    bmp_words[i] = formatted_pixel_color;
  return image_data;
}

bool IsImageRectOnScreen(PP_Resource graphics2d,
                         PP_Point origin,
                         PP_Size size,
                         ColorPremul color) {
  PP_Size size2d;
  PP_Bool dummy;
  CHECK(PP_TRUE == PPBGraphics2D()->Describe(graphics2d, &size2d, &dummy));

  void* bitmap = NULL;
  PP_Resource image = CreateImageData(size2d, kOpaqueBlack, &bitmap);

  PP_ImageDataDesc image_desc;
  CHECK(PP_TRUE == PPBImageData()->Describe(image, &image_desc));
  int32_t stride = image_desc.stride / kBytesPerPixel;  // width + padding.
  uint32_t expected_color = FormatColor(image_desc.format, color);
  CHECK(origin.x >= 0 && origin.y >= 0 &&
        (origin.x + size.width) <= stride &&
        (origin.y + size.height) <= image_desc.size.height);

  CHECK(PP_TRUE == PPBTestingPrivate()->ReadImageData(
      graphics2d, image, &kOrigin));
  bool found_error = false;
  for (int y = origin.y; y < origin.y + size.height && !found_error; y++) {
    for (int x = origin.x; x < origin.x + size.width && !found_error; x++) {
      uint32_t pixel_color = static_cast<uint32_t*>(bitmap)[stride * y + x];
      found_error = (pixel_color != expected_color);
    }
  }

  PPBCore()->ReleaseResource(image);
  return !found_error;
}
