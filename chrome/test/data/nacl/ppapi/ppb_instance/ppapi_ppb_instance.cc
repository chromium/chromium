// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/nacl/ppapi_test_lib/get_browser_interface.h"
#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"

namespace {


// Tests PPB_Instance::IsFullFrame().
void TestIsFullFrame() {
  // Note: IsFullFrame returning PP_TRUE is only possible when a plugin
  // is a content handler.  For coverage, see:
  // tests/ppapi_browser/extension_mime_handler/
  PP_Bool full_frame = PPBInstance()->IsFullFrame(pp_instance());
  EXPECT(full_frame == PP_FALSE);

  full_frame = PPBInstance()->IsFullFrame(kInvalidInstance);
  EXPECT(full_frame == PP_FALSE);

  TEST_PASSED;
}

void TestBindGraphics() {
  PP_Size size = PP_MakeSize(100, 100);

  PP_Resource graphics1 = PPBGraphics2D()->Create(
      pp_instance(), &size, PP_TRUE);
  PP_Resource graphics2 = PPBGraphics2D()->Create(
      pp_instance(), &size, PP_TRUE);

  EXPECT(graphics1 != kInvalidResource);
  EXPECT(graphics2 != kInvalidResource);

  PP_Bool ret = PPBInstance()->BindGraphics(pp_instance(), graphics1);
  EXPECT(ret == PP_TRUE);

  // We should be allowed to replace one device with another.
  ret = PPBInstance()->BindGraphics(pp_instance(), graphics2);
  EXPECT(ret == PP_TRUE);

  // This should fail because instance is not valid.
  ret = PPBInstance()->BindGraphics(kInvalidInstance, graphics1);
  EXPECT(ret == PP_FALSE);

  // This should fail because instance is not valid and graphics2 is bound.
  ret = PPBInstance()->BindGraphics(kInvalidInstance, graphics2);
  EXPECT(ret == PP_FALSE);

  // This is not a failure, binding resource 0 simply unbinds all devices.
  ret = PPBInstance()->BindGraphics(pp_instance(), kInvalidResource);
  EXPECT(ret == PP_TRUE);

  PP_Resource image_data = PPBImageData()->Create(
      pp_instance(), PP_IMAGEDATAFORMAT_RGBA_PREMUL, &size, PP_FALSE);
  EXPECT(image_data != kInvalidResource);

  // This should fail because the resource is the wrong type.
  ret = PPBInstance()->BindGraphics(pp_instance(), image_data);
  EXPECT(ret == PP_FALSE);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestIsFullFrame", TestIsFullFrame);
  RegisterTest("TestBindGraphics", TestBindGraphics);
}

void SetupPluginInterfaces() {
  // none
}
