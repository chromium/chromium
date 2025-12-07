// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

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

#if BUILDFLAG(ENABLE_OPENXR)
IN_PROC_MULTI_CLASS_BROWSER_TEST_F1(WebXrVrOpenXrBrowserTestWebXrDisabled,
                                    WebXrVrBrowserTestBase,
                                    TestWebXrDisabledWithoutFlagSet) {
  TestApiDisabledWithoutFlagSetImpl(t, "test_webxr_disabled_without_flag_set");
}
#endif  // BUILDFLAG(ENABLE_OPENXR)

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
  MockXRDeviceHookBase mock;
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
  MockXRDeviceHookBase mock;
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
  MockXRDeviceHookBase mock;
  t->LoadFileAndAwaitInitialization(
      "test_non_immersive_stops_during_immersive");
  t->ExecuteStepAndWait("stepBeforeImmersive()");
  t->EnterSessionWithUserGestureOrFail();
  t->ExecuteStepAndWait("stepDuringImmersive()");
  t->EndSessionOrFail();
  t->ExecuteStepAndWait("stepAfterImmersive()");
  t->EndTest();
}

// Tests that sessions can be repeatedly entered/exited.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestMultipleEntryFromBlinkEnd) {
  MockXRDeviceHookBase mock;
  t->LoadFileAndAwaitInitialization("generic_webxr_page");

  for (size_t i = 0; i < 5; i++) {
    t->EnterSessionWithUserGestureOrFail();
    mock.WaitNumFrames(5);
    t->EndSessionOrFail();
  }
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestEndSessionFromBlink) {
  MockXRDeviceHookBase transition_mock;
  t->LoadFileAndAwaitInitialization("test_webxr_presentation_ended");
  t->EnterSessionWithUserGestureOrFail();

  // Wait for JavaScript to submit at least one frame.
  ASSERT_TRUE(
      t->PollJavaScriptBoolean("hasPresentedFrame", t->kPollTimeoutMedium))
      << "No frame submitted";
  t->EndSessionOrFail();
  // Tell JavaScript that it is done with the test.
  t->WaitOnJavaScriptStep();
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

IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest, TestSessionExited) {
  // Set the device up to reject the session request. This should translate to
  // immediately translating the device to the "Exited" state.
  MockXRDeviceHookBase transition_mock;
  transition_mock.SetCanCreateSession(false);

  this->LoadFileAndAwaitInitialization("generic_webxr_page");

  // Not using "EnterSessionWithUserGestureOrFail" because we actually expect
  // the session to reject. So instead we check that an error got set.
  this->EnterSessionWithUserGesture();
  this->PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].error != null", kPollTimeoutLong);

  // Tell JavaScript that it is done with the test.
  this->RunJavaScriptOrFail("done()");
  this->EndTest();
}

// Tests that sessions can be repeatedly entered/exited.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest,
                       TestMultipleEntryFromDeviceEnd) {
  MockXRDeviceHookBase mock;
  LoadFileAndAwaitInitialization("generic_webxr_page");

  for (size_t i = 0; i < 5; i++) {
    EnterSessionWithUserGestureOrFail();
    mock.WaitNumFrames(5);
    device_test::mojom::EventData data = {};
    data.type = device_test::mojom::EventType::kSessionLost;
    mock.PopulateEvent(data);
    WaitForSessionEndOrFail();
  }

  // EndTest();
}

IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest,
                       TestVisibilityMaskChangeEventReceived) {
  MockXRDeviceHookBase mock;
  device::VisibilityMaskData visibility_mask{
      .vertices = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f}, .indices = {0, 1, 2}};
  mock.SetVisibilityMaskForTesting(0, visibility_mask);
  mock.SetVisibilityMaskForTesting(1, visibility_mask);

  LoadFileAndAwaitInitialization("test_visibility_mask_change_event");
  EnterSessionWithUserGestureOrFail();
  PollJavaScriptBooleanOrFail("visibilityMaskChangeEventCount > 0",
                              kPollTimeoutMedium);
}

IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest, TestVisibilityChanged) {
  MockXRDeviceHookBase transition_mock;
  LoadFileAndAwaitInitialization("webxr_test_visibility_changed");
  EnterSessionWithUserGestureOrFail();

  // Wait for JavaScript to submit at least one frame.
  ASSERT_TRUE(PollJavaScriptBoolean("hasPresentedFrame", kPollTimeoutMedium))
      << "No frame submitted";

  PollJavaScriptBooleanOrFail("isVisibilityEqualTo('visible')",
                              kPollTimeoutMedium);

  RunJavaScriptOrFail("subscribeToVisibilityChange()");

  device_test::mojom::EventData event_data = {};
  event_data.type = device_test::mojom::EventType::kVisibilityVisibleBlurred;
  transition_mock.PopulateEvent(event_data);

  PollJavaScriptBooleanOrFail("visibility_change_count == 1",
                              kPollTimeoutMedium);
  PollJavaScriptBooleanOrFail("isVisibilityEqualTo('visible-blurred')",
                              kPollTimeoutMedium);
  RunJavaScriptOrFail("done()");
  EndTest();
}

IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTest, TestFramesWhenVisibleBlurred) {
  MockXRDeviceHookBase transition_mock;
  LoadFileAndAwaitInitialization("webxr_test_visibility_changed");
  EnterSessionWithUserGestureOrFail();

  // Wait for JavaScript to submit at least one frame.
  ASSERT_TRUE(PollJavaScriptBoolean("hasPresentedFrame", kPollTimeoutMedium))
      << "No frame submitted";

  device_test::mojom::EventData event_data = {};
  event_data.type = device_test::mojom::EventType::kVisibilityVisibleBlurred;
  transition_mock.PopulateEvent(event_data);

  PollJavaScriptBooleanOrFail("isVisibilityEqualTo('visible-blurred')",
                              kPollTimeoutMedium);
  // Ensure that we still get a couple of frames after being set to
  // visible-blurred.
  transition_mock.WaitNumFrames(2);
  RunJavaScriptOrFail("done()");
  EndTest();
}
#endif  // BUILDFLAG(ENABLE_OPENXR)

}  // namespace vr
