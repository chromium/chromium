// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

// Tests that WebXR can still get an inline identity reference space when there
// are no runtimes available.
IN_PROC_BROWSER_TEST_F(WebXrVrRuntimelessBrowserTest,
                       TestInlineIdentityAlwaysAvailable) {
  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_inline_viewer_available"));
  WaitOnJavaScriptStep();
  EndTest();
}

#if BUILDFLAG(ENABLE_VR)
IN_PROC_BROWSER_TEST_F(WebXrVrRuntimelessBrowserTestSensorless,
                       TestSensorlessRejections) {
  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_local_floor_reference_space_rejects"));
  WaitOnJavaScriptStep();
  EndTest();
}
#endif
}  // namespace vr
