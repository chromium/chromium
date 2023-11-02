// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

// The permission flow isn't specific to any particular runtime; so it's okay to
// keep it specific to one runtime; however, ShownCount and SetupObservers could
// move to a higher level and tests could be run on all supported runtimes if
// desired.
class WebXrVrPermissionsBrowserTest
    : public WebXrVrOpenXrBrowserTestBase,
      public permissions::PermissionRequestManager::Observer {
 public:
  WebXrVrPermissionsBrowserTest() = default;
  ~WebXrVrPermissionsBrowserTest() override = default;

  void SetupObservers() { GetPermissionRequestManager()->AddObserver(this); }

  uint32_t ShownCount() { return shown_count_; }

 private:
  void OnPromptAdded() override { shown_count_++; }

  uint32_t shown_count_ = 0u;
};

// Tests that WebXR sessions can be created when permission is granted.
IN_PROC_BROWSER_TEST_F(WebXrVrPermissionsBrowserTest,
                       TestGrantingPermissionCreatesSession) {
  permission_auto_response_ =
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL;

  LoadFileAndAwaitInitialization("generic_webxr_page");
  SetupObservers();

  EnterSessionWithUserGestureOrFail();

  ASSERT_EQ(ShownCount(), 1u) << "Permission prompt should've been shown once";
}

// Tests that a session is not created if the user explicitly denies permission.
IN_PROC_BROWSER_TEST_F(WebXrVrPermissionsBrowserTest,
                       TestDenyingPermissionFailsSessionCreation) {
  permission_auto_response_ =
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL;

  LoadFileAndAwaitInitialization("test_webxr_permission");
  SetupObservers();
  EnterSessionWithUserGesture();
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].error != null",
      WebXrVrBrowserTestBase::kPollTimeoutMedium);
  RunJavaScriptOrFail("verifyPermissionDeniedError(sessionTypes.IMMERSIVE)");
  AssertNoJavaScriptErrors();

  ASSERT_EQ(ShownCount(), 1u) << "Permission prompt should've been shown once";
}

// Tests that a session is not created if the user explicitly closes the
// dialog.
IN_PROC_BROWSER_TEST_F(WebXrVrPermissionsBrowserTest,
                       TestDismissingPromptCloseFailsSessionCreation) {
  permission_auto_response_ =
      permissions::PermissionRequestManager::AutoResponseType::DISMISS;

  LoadFileAndAwaitInitialization("test_webxr_permission");
  SetupObservers();
  EnterSessionWithUserGesture();
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].error != null",
      WebXrVrBrowserTestBase::kPollTimeoutMedium);
  RunJavaScriptOrFail("verifyPermissionDeniedError(sessionTypes.IMMERSIVE)");
  AssertNoJavaScriptErrors();

  ASSERT_EQ(ShownCount(), 1u) << "Permission prompt should've been shown once";
}

// Tests that requesting the same type of session only prompts once.
IN_PROC_BROWSER_TEST_F(WebXrVrPermissionsBrowserTest, TestPermissionPersists) {
  permission_auto_response_ =
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL;

  LoadFileAndAwaitInitialization("generic_webxr_page");
  SetupObservers();

  EnterSessionWithUserGestureOrFail();
  EndSessionOrFail();

  // Since the permission from the earlier prompt should be persisted,
  // requesting an XR session a second time should not prompt the user, but
  // should create a valid session.
  EnterSessionWithUserGestureOrFail();

  // Validate that the permission prompt has only been shown once since the
  // start of this test.
  ASSERT_EQ(ShownCount(), 1u)
      << "Permission prompt should've only been shown once";
}

// Verify that inline with no session parameters doesn't prompt for permission.
IN_PROC_BROWSER_TEST_F(WebXrVrPermissionsBrowserTest,
                       TestPermissionNotNeededForInline) {
  permission_auto_response_ =
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL;

  LoadFileAndAwaitInitialization("test_webxr_permission");
  SetupObservers();
  RunJavaScriptOrFail("requestMagicWindowSession()");

  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.MAGIC_WINDOW].currentSession != null",
      WebXrVrBrowserTestBase::kPollTimeoutLong);

  // Validate that the permission prompt has not been shown since this test
  // began.
  ASSERT_EQ(ShownCount(), 0u) << "Permission prompt should not have been shown";
}

}  // namespace vr
