// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
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
  t->LoadUrlAndAwaitInitialization(
      t->GetEmbeddedServerUrlForHtmlTestFile("generic_webxr_page"));
  t->EnterSessionWithUserGestureOrFail();
  // Use location instead of camera/microphone since those automatically reject
  // if a suitable device is not connected.
  // TODO(bsheedy): Find a way to support more permission types (maybe use
  // MockPermissionPromptFactory?).
  t->RunJavaScriptOrFail(
      "navigator.geolocation.getCurrentPosition( ()=>{}, ()=>{} )");
  auto utils = UiUtils::Create();
  utils->PerformActionAndWaitForVisibilityStatus(
      UserFriendlyElementName::kWebXrExternalPromptNotification,
      true /* visible */, base::DoNothing::Once());
}

// TODO(https://crbug.com/920697): Add tests verifying the notification
// disappears when the permission is accepted/denied once we can query element
// visibility at any time using PermissionRequestManagerTestApi.

}  // namespace vr
