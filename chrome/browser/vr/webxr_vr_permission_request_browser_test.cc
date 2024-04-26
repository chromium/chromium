// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "chrome/browser/vr/ui_test_input.h"

// Browser tests for testing permission requests that occur during a WebXR
// immersive session.

namespace vr {

// Tests that permission requests that occur when in an immersive session cause
// a notification to appear telling the user that a permission request is
// visible in the browser and that closing the browser while this is still
// displayed does not cause any issues.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(
    TestInSessionPermissionNotificationCloseWhileVisible) {
  // We need to use a local server for permission requests to not hit a DCHECK.
  t->LoadFileAndAwaitInitialization("generic_webxr_page");
  t->EnterSessionWithUserGestureOrFail();
  // Use location instead of camera/microphone since those automatically reject
  // if a suitable device is not connected.
  // TODO(bsheedy): Find a way to support more permission types (maybe use
  // MockPermissionPromptFactory?).

  // AutoResponseForTest is overridden when requesting a session. We don't want
  // to change that as we want anything necessary to request a session to get
  // granted. However, we want no action to be taken now so that the prompt for
  // location comes up and does not get dismissed.
  t->GetPermissionRequestManager()->set_auto_response_for_test(
      permissions::PermissionRequestManager::NONE);
  t->RunJavaScriptOrFail(
      "navigator.geolocation.getCurrentPosition( ()=>{}, ()=>{} )");
  base::RunLoop().RunUntilIdle();
  auto utils = UiUtils::Create();
  utils->WaitForVisibilityStatus(
      UserFriendlyElementName::kWebXrExternalPromptNotification,
      true /* visible */);
}

// TODO(crbug.com/41434932): Add tests verifying the notification
// disappears when the permission is accepted/denied once we can query element
// visibility at any time using PermissionRequestManagerTestApi.

}  // namespace vr
