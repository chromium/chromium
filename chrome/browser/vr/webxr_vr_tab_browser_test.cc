// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "url/url_constants.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr/WebXrVrTabTest.java.
// End-to-end tests for testing WebXR's interaction with multiple tabs.
namespace vr {

// Tests that non-focused tabs cannot get pose information from WebXR.
void TestPoseDataUnfocusedTabImpl(WebXrVrBrowserTestBase* t,
                                  std::string filename) {
  t->LoadFileAndAwaitInitialization(filename);
  t->ExecuteStepAndWait("stepCheckFrameDataWhileFocusedTab()");
  auto* first_tab_web_contents = t->GetCurrentWebContents();
  t->OpenNewTab(url::kAboutBlankURL);
  t->ExecuteStepAndWait("stepCheckFrameDataWhileNonFocusedTab()",
                        first_tab_web_contents);
  t->EndTest(first_tab_web_contents);
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestPoseDataUnfocusedTab) {
  TestPoseDataUnfocusedTabImpl(t, "webxr_test_pose_data_unfocused_tab");
}

}  // namespace vr
