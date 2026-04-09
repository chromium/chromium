// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENXR)

namespace vr {

// Test all kinds of layers in WebXR. This test requests the 'layers' feature.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestLayers) {
  UiUtils::DisableOverlayForTesting();
  MockXRDeviceHookBase mock;

  t->LoadFileAndAwaitInitialization("test_openxr_layers");
  t->EnterSessionWithUserGestureOrFail();

  t->WaitOnJavaScriptStep();
  t->AssertNoJavaScriptErrors();

  t->EndTest();
}

}  // namespace vr

#endif  // BUILDFLAG(ENABLE_OPENXR)
