// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/common/content_switches.h"
#include "device/vr/buildflags/buildflags.h"

namespace vr {

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestNoStalledFrameLoop) {
  MockXRDeviceHookBase my_mock;

  // Load the test page, and enter presentation.
  t->LoadFileAndAwaitInitialization("webxr_no_stalled_frame_loop");
  t->EnterSessionWithUserGestureOrFail();

  // Wait for 2 frames to be submitted back to the device, but the js frame loop
  // should've only been called once.
  my_mock.WaitForTotalFrameCount(2);
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail("frame_count === 1"));

  // Now restart the frame loop and wait for another frame to get submitted.
  t->RunJavaScriptOrFail("setBaseLayer()");
  t->PollJavaScriptBooleanOrFail("frame_count >= 2",
                                 XrBrowserTestBase::kPollTimeoutMedium);

  t->AssertNoJavaScriptErrors();
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestLateSetOfBaseLayer) {
  MockXRDeviceHookBase my_mock;

  // Load the test page, and enter presentation.
  t->LoadFileAndAwaitInitialization("webxr_set_base_layer_late");
  t->EnterSessionWithUserGestureOrFail();

  // Poll and have the javascript yield for 500 ms.  This should give us enough
  // time for any frame requests that were going to propagate to propagate.
  t->RunJavaScriptOrFail("delayMilliseconds(500)");
  t->PollJavaScriptBooleanOrFail("delay_ended");

  // No frames should have been submitted to either the JS or the runtime.
  ASSERT_TRUE(t->RunJavaScriptAndExtractBoolOrFail("frame_count === 0"));
  ASSERT_EQ(my_mock.GetFrameCount(), 0u);

  // Now restart the frame loop and wait for a frame to get submitted.
  t->RunJavaScriptOrFail("setBaseLayer()");
  t->PollJavaScriptBooleanOrFail("frame_count >= 1",
                                 XrBrowserTestBase::kPollTimeoutMedium);

  t->AssertNoJavaScriptErrors();
}

#if BUILDFLAG(ENABLE_OPENXR)
class WebXrVrOpenXrFrameRateBrowserTest : public WebXrVrOpenXrBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebXrVrOpenXrBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "WebXRFrameRate");
  }
};

// The fake runtime offers 72, 90 and 120 Hz, and starts at 90 Hz.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrFrameRateBrowserTest, TestFrameRate) {
  MockXRDeviceHookBase my_mock;
  LoadFileAndAwaitInitialization("test_webxr_frame_rate");
  EnterSessionWithUserGestureOrFail();

  RunJavaScriptOrFail(
      "assert_array_equals(Array.from(immersiveSession().supportedFrameRates), "
      "[72, 90, 120])");
  PollJavaScriptBooleanOrFail("immersiveSession().frameRate === 90",
                              kPollTimeoutMedium);

  RunJavaScriptOrFail("updateTargetFrameRate(60)");
  PollJavaScriptBooleanOrFail("updateResult === 'TypeError'",
                              kPollTimeoutShort);

  RunJavaScriptOrFail("updateTargetFrameRate(120)");
  PollJavaScriptBooleanOrFail("updateResult === 'resolved'",
                              kPollTimeoutMedium);
  PollJavaScriptBooleanOrFail("immersiveSession().frameRate === 120",
                              kPollTimeoutMedium);
  RunJavaScriptOrFail("assert_equals(frameRateChanges, 1)");

  RunJavaScriptOrFail("done()");
  EndTest();
}

// updateTargetFrameRate() resolves without new frames.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrFrameRateBrowserTest,
                       TestFrameRateUpdateWithoutFrames) {
  MockXRDeviceHookBase my_mock;
  LoadFileAndAwaitInitialization("test_webxr_frame_rate");
  EnterSessionWithUserGestureOrFail();
  PollJavaScriptBooleanOrFail("immersiveSession().frameRate === 90",
                              kPollTimeoutMedium);

  RunJavaScriptOrFail("stopFrameLoop()");
  PollJavaScriptBooleanOrFail("frameLoopStopped", kPollTimeoutShort);
  RunJavaScriptOrFail("updateTargetFrameRate(120)");
  PollJavaScriptBooleanOrFail("updateResult === 'resolved'",
                              kPollTimeoutMedium);

  RunJavaScriptOrFail("done()");
  EndTest();
}

// Ending the session rejects a pending updateTargetFrameRate().
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrFrameRateBrowserTest,
                       TestEndingSessionRejectsPendingFrameRateUpdate) {
  MockXRDeviceHookBase my_mock;
  LoadFileAndAwaitInitialization("test_webxr_frame_rate");
  EnterSessionWithUserGestureOrFail();
  PollJavaScriptBooleanOrFail("immersiveSession().frameRate === 90",
                              kPollTimeoutMedium);

  RunJavaScriptOrFail("updateTargetFrameRateAndEndSession(120)");
  PollJavaScriptBooleanOrFail("updateResult === 'InvalidStateError'",
                              kPollTimeoutShort);

  RunJavaScriptOrFail("done()");
  EndTest();
}
#endif  // BUILDFLAG(ENABLE_OPENXR)

}  // namespace vr
