// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {
// Tests that WebXR sessions can be created when permission is granted.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTestBase,
                       TestGrantingPermissionCreatesSession) {
  MockXRDeviceHookBase mock;
  SetPermissionAutoResponse(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  LoadFileAndAwaitInitialization("generic_webxr_page");

  EnterSessionWithUserGestureOrFail();

  ASSERT_EQ(GetPermissionPromptFactory()->show_count(), 1)
      << "Permission prompt should've been shown once";
}

// Tests that a session is not created if the user explicitly denies permission.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTestBase,
                       TestDenyingPermissionFailsSessionCreation) {
  MockXRDeviceHookBase mock;
  SetPermissionAutoResponse(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  LoadFileAndAwaitInitialization("test_webxr_permission");
  EnterSessionWithUserGesture();
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].error != null",
      WebXrVrBrowserTestBase::kPollTimeoutMedium);
  RunJavaScriptOrFail("verifyPermissionDeniedError(sessionTypes.IMMERSIVE)");
  AssertNoJavaScriptErrors();

  ASSERT_EQ(GetPermissionPromptFactory()->show_count(), 1)
      << "Permission prompt should've been shown once";
}

// Tests that a session is not created if the user explicitly closes the
// dialog.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTestBase,
                       TestDismissingPromptCloseFailsSessionCreation) {
  MockXRDeviceHookBase mock;
  SetPermissionAutoResponse(
      permissions::PermissionRequestManager::AutoResponseType::DISMISS);

  LoadFileAndAwaitInitialization("test_webxr_permission");
  EnterSessionWithUserGesture();
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].error != null",
      WebXrVrBrowserTestBase::kPollTimeoutMedium);
  RunJavaScriptOrFail("verifyPermissionDeniedError(sessionTypes.IMMERSIVE)");
  AssertNoJavaScriptErrors();

  ASSERT_EQ(GetPermissionPromptFactory()->show_count(), 1)
      << "Permission prompt should've been shown once";
}

// Tests that requesting the same type of session only prompts once.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTestBase, TestPermissionPersists) {
  MockXRDeviceHookBase mock;

  SetPermissionAutoResponse(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  LoadFileAndAwaitInitialization("generic_webxr_page");

  EnterSessionWithUserGestureOrFail();

  // End the session from the device process, due to potential racy behavior
  // when ending a session from blink.
  // TODO(https://crbug.com/235526581): Fix end session behavior.
  device_test::mojom::EventData data = {};
  data.type = device_test::mojom::EventType::kSessionLost;
  mock.PopulateEvent(data);
  WaitForSessionEndOrFail();

  // Since the permission from the earlier prompt should be persisted,
  // requesting an XR session a second time should not prompt the user, but
  // should create a valid session.
  EnterSessionWithUserGestureOrFail();

  // Validate that the permission prompt has only been shown once since the
  // start of this test.
  ASSERT_EQ(GetPermissionPromptFactory()->show_count(), 1)
      << "Permission prompt should've only been shown once";
}

// Verify that inline with no session parameters doesn't prompt for permission.
IN_PROC_BROWSER_TEST_F(WebXrVrOpenXrBrowserTestBase,
                       TestPermissionNotNeededForInline) {
  SetPermissionAutoResponse(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  LoadFileAndAwaitInitialization("test_webxr_permission");
  RunJavaScriptOrFail("requestMagicWindowSession()");

  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.MAGIC_WINDOW].currentSession != null",
      WebXrVrBrowserTestBase::kPollTimeoutLong);

  // Validate that the permission prompt has not been shown since this test
  // began.
  ASSERT_EQ(GetPermissionPromptFactory()->show_count(), 0)
      << "Permission prompt should not have been shown";
}

}  // namespace vr
