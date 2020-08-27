// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/vr/buildflags/buildflags.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr/WebXrVrTransitionTest.java.
// End-to-end tests for transitioning between immersive and non-immersive
// sessions.

namespace vr {

// Tests that WebXR is not exposed if the flag is not on and the page does
// not have an origin trial token.
void TestApiDisabledWithoutFlagSetImpl(WebXrVrBrowserTestBase* t,
                                       std::string filename) {
  t->LoadFileAndAwaitInitialization(filename);
  t->WaitOnJavaScriptStep();
  t->EndTest();
}

// Tests that WebXR does not return any devices if all runtime support is
// disabled.
IN_PROC_BROWSER_TEST_F(WebXrVrRuntimelessBrowserTest,
                       TestWebXrNoDevicesWithoutRuntime) {
  LoadFileAndAwaitInitialization("test_webxr_does_not_return_device");
  WaitOnJavaScriptStep();
  EndTest();
}

// Windows-specific tests.
#ifdef OS_WIN

#if BUILDFLAG(ENABLE_OPENXR)
IN_PROC_MULTI_CLASS_BROWSER_TEST_F2(WebXrVrWmrBrowserTestWebXrDisabled,
                                    WebXrVrOpenXrBrowserTestWebXrDisabled,
                                    WebXrVrBrowserTestBase,
                                    TestWebXrDisabledWithoutFlagSet) {
#else
IN_PROC_MULTI_CLASS_BROWSER_TEST_F1(WebXrVrWmrBrowserTestWebXrDisabled,
                                    WebXrVrBrowserTestBase,
                                    TestWebXrDisabledWithoutFlagSet) {
#endif  // BUILDFLAG(ENABLE_OPENXR)
  TestApiDisabledWithoutFlagSetImpl(t, "test_webxr_disabled_without_flag_set");
}

// Tests that window.requestAnimationFrame continues to fire when we have a
// non-immersive WebXR session.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestWindowRafFiresDuringNonImmersiveSession) {
  t->LoadFileAndAwaitInitialization(
      "test_window_raf_fires_during_non_immersive_session");
  t->WaitOnJavaScriptStep();
  t->EndTest();
}

// Tests that a successful requestPresent or requestSession call enters
// an immersive session.
void TestPresentationEntryImpl(WebXrVrBrowserTestBase* t,
                               std::string filename) {
  t->LoadFileAndAwaitInitialization(filename);
  t->EnterSessionWithUserGestureOrFail();
  t->AssertNoJavaScriptErrors();
}

WEBXR_VR_ALL_RUNTIMES_PLUS_INCOGNITO_BROWSER_TEST_F(
    TestRequestSessionEntersVr) {
  TestPresentationEntryImpl(t, "generic_webxr_page");
}

// Tests that window.requestAnimationFrame continues to fire while in
// WebXR presentation since the tab is still visible.
void TestWindowRafFiresWhilePresentingImpl(WebXrVrBrowserTestBase* t,
                                           std::string filename) {
  t->LoadFileAndAwaitInitialization(filename);
  t->ExecuteStepAndWait("stepVerifyBeforePresent()");
  t->EnterSessionWithUserGestureOrFail();
  t->ExecuteStepAndWait("stepVerifyDuringPresent()");
  t->EndSessionOrFail();
  t->ExecuteStepAndWait("stepVerifyAfterPresent()");
  t->EndTest();
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestWindowRafFiresWhilePresenting) {
  TestWindowRafFiresWhilePresentingImpl(
      t, "webxr_test_window_raf_fires_while_presenting");
}

// Tests that non-immersive sessions stop receiving rAFs during an immersive
// session, but resume once the immersive session ends.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestNonImmersiveStopsDuringImmersive) {
  t->LoadFileAndAwaitInitialization(
      "test_non_immersive_stops_during_immersive");
  t->ExecuteStepAndWait("stepBeforeImmersive()");
  t->EnterSessionWithUserGestureOrFail();
  t->ExecuteStepAndWait("stepDuringImmersive()");
  t->EndSessionOrFail();
  t->ExecuteStepAndWait("stepAfterImmersive()");
  t->EndTest();
}

#if BUILDFLAG(ENABLE_OPENXR)
// Tests that WebXR session ends when certain events are received.
void TestWebXRSessionEndWhenEventTriggered(
    WebXrVrBrowserTestBase* t,
    device_test::mojom::EventType event_type) {
  MockXRDeviceHookBase transition_mock;
  t->LoadFileAndAwaitInitialization("test_webxr_presentation_ended");
  t->EnterSessionWithUserGestureOrFail();

  // Wait for JavaScript to submit at least one frame.
  ASSERT_TRUE(
      t->PollJavaScriptBoolean("hasPresentedFrame", t->kPollTimeoutMedium))
      << "No frame submitted";
  device_test::mojom::EventData data = {};
  data.type = event_type;
  transition_mock.PopulateEvent(data);
  // Tell JavaScript that it is done with the test.
  t->WaitOnJavaScriptStep();
  t->EndTest();
}

IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest, TestSessionEnded) {
  TestWebXRSessionEndWhenEventTriggered(
      this, device_test::mojom::EventType::kSessionLost);
}

IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest, TestInsanceLost) {
  TestWebXRSessionEndWhenEventTriggered(
      this, device_test::mojom::EventType::kInstanceLost);
}

IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest, TestVisibilityChanged) {
  MockXRDeviceHookBase transition_mock;
  this->LoadFileAndAwaitInitialization("webxr_test_visibility_changed");
  this->EnterSessionWithUserGestureOrFail();

  // Wait for JavaScript to submit at least one frame.
  ASSERT_TRUE(this->PollJavaScriptBoolean("hasPresentedFrame",
                                          this->kPollTimeoutMedium))
      << "No frame submitted";

  this->PollJavaScriptBooleanOrFail("isVisibilityEqualTo('visible')",
                                    this->kPollTimeoutMedium);

  device_test::mojom::EventData event_data = {};
  event_data.type = device_test::mojom::EventType::kVisibilityVisibleBlurred;
  transition_mock.PopulateEvent(event_data);

  // TODO(crbug.com/1002742): visible-blurred is forced to hidden in WebXR
  this->PollJavaScriptBooleanOrFail("isVisibilityEqualTo('hidden')",
                                    this->kPollTimeoutMedium);
  this->RunJavaScriptOrFail("done()");
  this->EndTest();
}
#endif  // BUILDFLAG(ENABLE_OPENXR)

#endif  // OS_WIN

}  // namespace vr
