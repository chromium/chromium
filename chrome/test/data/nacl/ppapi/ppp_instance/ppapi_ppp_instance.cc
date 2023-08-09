// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include <vector>

#include "chrome/test/data/nacl/ppapi_test_lib/get_browser_interface.h"
#include "chrome/test/data/nacl/ppapi_test_lib/test_interface.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/c/ppp_instance.h"

namespace {

PP_Bool DidCreate(PP_Instance instance,
                  uint32_t argc,
                  const char* argn[],
                  const char* argv[]) {
  printf("--- PPP_Instance::DidCreate\n");
  PP_Bool status = DidCreateDefault(instance, argc, argn, argv);
  // TEST_PASSED has no effect from this function.
  // But as long as the plugin loads and tests are run, we know this succeeded.
  // See ppapi_browser/bad for failing DidCreate.
  return status;
}

// This should never be called.
void DidDestroy(PP_Instance instance) {
  printf("--- PPP_Instance::DidDestroy\n");
  CHECK(instance == pp_instance());
  NACL_NOTREACHED();
}

void DidChangeView(PP_Instance instance, PP_Resource view) {
  printf("--- PPP_Instance::DidChangeView\n");
  EXPECT(instance == pp_instance());

  PP_Rect clip;
  PPBView()->GetClipRect(view, &clip);
  EXPECT(clip.point.x == 0 && clip.point.y == 0);

  // These are based on embed dimensions.
  PP_Rect position;
  PPBView()->GetRect(view, &position);
  EXPECT(position.size.width == 15 && clip.size.width == 15);
  EXPECT(position.size.height == 20 && clip.size.height == 20);

  TEST_PASSED;
}

void DidChangeFocus(PP_Instance instance,
                    PP_Bool has_focus) {
  printf("--- PPP_Instance::DidChangeFocus has_focus=%d\n", has_focus);
  // There should be no focus on load, so this will trigger when we gain it
  // and then release it and so on.
  static bool expected_has_focus = true;
  EXPECT(instance == pp_instance());
  EXPECT(has_focus == expected_has_focus);
  expected_has_focus = !expected_has_focus;

  TEST_PASSED;
}

PP_Bool HandleDocumentLoad(PP_Instance instance,
                           PP_Resource url_loader) {
  // Only called for full-frame plugins. For coverage see:
  // tests/ppapi_browser/extension_mime_handler/
  NACL_NOTREACHED();
}

const PPP_Instance ppp_instance_interface = {
  DidCreate,
  DidDestroy,
  DidChangeView,
  DidChangeFocus,
  HandleDocumentLoad
};

}  // namespace


void SetupTests() {
  // Each PPP_Instance function called by the browser acts as a test.
}

void SetupPluginInterfaces() {
  RegisterPluginInterface(PPP_INSTANCE_INTERFACE, &ppp_instance_interface);
}
