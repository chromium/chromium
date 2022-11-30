// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/test/browser_test.h"

namespace vr {

// Tests that WebXR does not return any devices if all runtime support is
// disabled.
IN_PROC_BROWSER_TEST_F(WebXrVrRuntimelessBrowserTest,
                       TestWebXrNoDevicesWithoutRuntime) {
  LoadFileAndAwaitInitialization("test_webxr_does_not_return_device");
  WaitOnJavaScriptStep();
  EndTest();
}

}  // namespace vr
