// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/webvr_browser_test.h"

#include "build/build_config.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/test/browser_test_utils.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr/WebXrVrTransitionTest.java.
// End-to-end tests for transitioning between immersive and non-immersive
// sessions.

namespace vr {

// Tests that a successful requestPresent or requestSession call enters
// an immersive session.
void TestPresentationEntryImpl(WebXrVrBrowserTestBase* t,
                               std::string filename) {
  t->LoadUrlAndAwaitInitialization(t->GetHtmlTestFile(filename));
  t->EnterSessionWithUserGestureOrFail();
  t->AssertNoJavaScriptErrors();
}

IN_PROC_BROWSER_TEST_F(WebVrBrowserTestStandard,
                       REQUIRES_GPU(TestRequestPresentEntersVr)) {
  TestPresentationEntryImpl(this, "generic_webvr_page");
}
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard,
                       REQUIRES_GPU(TestRequestSessionEntersVr)) {
  TestPresentationEntryImpl(this, "generic_webxr_page");
}

// Tests that window.requestAnimationFrame continues to fire while in
// WebVR/WebXR presentation since the tab is still visible.
void TestWindowRafFiresWhilePresentingImpl(WebXrVrBrowserTestBase* t,
                                           std::string filename) {
  t->LoadUrlAndAwaitInitialization(t->GetHtmlTestFile(filename));
  t->ExecuteStepAndWait("stepVerifyBeforePresent()");
  t->EnterSessionWithUserGestureOrFail();
  t->ExecuteStepAndWait("stepVerifyDuringPresent()");
  t->EndSessionOrFail();
  t->ExecuteStepAndWait("stepVerifyAfterPresent()");
  t->EndTest();
}

IN_PROC_BROWSER_TEST_F(WebVrBrowserTestStandard,
                       REQUIRES_GPU(TestWindowRafFiresWhilePresenting)) {
  TestWindowRafFiresWhilePresentingImpl(
      this, "test_window_raf_fires_while_presenting");
}
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard,
                       REQUIRES_GPU(TestWindowRafFiresWhilePresenting)) {
  TestWindowRafFiresWhilePresentingImpl(
      this, "webxr_test_window_raf_fires_while_presenting");
}

// Tests that WebVR/WebXR is not exposed if the flag is not on and the page does
// not have an origin trial token. Since the API isn't actually used, we can
// remove the GPU requirement.
void TestApiDisabledWithoutFlagSetImpl(WebXrVrBrowserTestBase* t,
                                       std::string filename) {
  t->LoadUrlAndAwaitInitialization(t->GetHtmlTestFile(filename));
  t->WaitOnJavaScriptStep();
  t->EndTest();
}

IN_PROC_BROWSER_TEST_F(WebVrBrowserTestWebVrDisabled,
                       TestWebVrDisabledWithoutFlagSet) {
  TestApiDisabledWithoutFlagSetImpl(this,
                                    "test_webvr_disabled_without_flag_set");
}
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestWebXrDisabled,
                       TestWebXrDisabledWithoutFlagSet) {
  TestApiDisabledWithoutFlagSetImpl(this,
                                    "test_webxr_disabled_without_flag_set");
}

// Tests that WebVR does not return any devices if OpenVR support is disabled.
// Since WebVR isn't actually used, we can remove the GPU requirement.
IN_PROC_BROWSER_TEST_F(WebVrBrowserTestOpenVrDisabled,
                       TestWebVrNoDevicesWithoutOpenVr) {
  LoadUrlAndAwaitInitialization(GetHtmlTestFile("generic_webvr_page"));
  EXPECT_FALSE(XrDeviceFound())
      << "Found a VRDisplay even with OpenVR disabled";
  AssertNoJavaScriptErrors();
}

// Tests that WebXR does not return any devices if OpenVR support is disabled.
// Since WebXR isn't actually used, we can remove the GPU requirement.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestOpenVrDisabled,
                       TestWebXrNoDevicesWithoutOpenVr) {
  LoadUrlAndAwaitInitialization(
      GetHtmlTestFile("test_webxr_does_not_return_device"));
  WaitOnJavaScriptStep();
  EndTest();
}

// Tests that window.requestAnimationFrame continues to fire when we have a
// non-immersive WebXR session.
IN_PROC_BROWSER_TEST_F(
    WebXrVrBrowserTestStandard,
    REQUIRES_GPU(TestWindowRafFiresDuringNonImmersiveSession)) {
  LoadUrlAndAwaitInitialization(
      GetHtmlTestFile("test_window_raf_fires_during_non_immersive_session"));
  WaitOnJavaScriptStep();
  EndTest();
}

// Tests that non-immersive sessions stop receiving rAFs during an immersive
// session, but resume once the immersive session ends.
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard,
                       REQUIRES_GPU(TestNonImmersiveStopsDuringImmersive)) {
  LoadUrlAndAwaitInitialization(
      GetHtmlTestFile("test_non_immersive_stops_during_immersive"));
  ExecuteStepAndWait("stepBeforeImmersive()");
  EnterSessionWithUserGestureOrFail();
  ExecuteStepAndWait("stepDuringImmersive()");
  EndSessionOrFail();
  ExecuteStepAndWait("stepAfterImmersive()");
  EndTest();
}

}  // namespace vr
