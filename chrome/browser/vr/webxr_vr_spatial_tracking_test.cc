// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cfi_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/test/browser_test.h"

namespace vr {

// Tests that WebXR can still get an inline identity reference space when there
// are no runtimes available.

// TODO(crbug.com/354355009): Flaky on Linux CFI
#if BUILDFLAG(CFI_ICALL_CHECK) && BUILDFLAG(IS_LINUX)
#define MAYBE_TestInlineIdentityAlwaysAvailable \
  DISABLED_TestInlineIdentityAlwaysAvailable
#else
#define MAYBE_TestInlineIdentityAlwaysAvailable \
  TestInlineIdentityAlwaysAvailable
#endif
IN_PROC_BROWSER_TEST_F(WebXrVrRuntimelessBrowserTest,
                       MAYBE_TestInlineIdentityAlwaysAvailable) {
  GetCurrentWebContents()->Focus();
  LoadFileAndAwaitInitialization("test_inline_viewer_available");
  WaitOnJavaScriptStep();
  EndTest();
}

#if BUILDFLAG(ENABLE_VR)
IN_PROC_BROWSER_TEST_F(WebXrVrRuntimelessBrowserTestSensorless,
                       TestSensorlessRejections) {
  LoadFileAndAwaitInitialization("test_local_floor_reference_space_rejects");
  WaitOnJavaScriptStep();
  EndTest();
}
#endif
}  // namespace vr
