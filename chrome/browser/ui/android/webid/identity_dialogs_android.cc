// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialogs.h"

#include <string>
#include "base/callback.h"
#include "base/notreached.h"
#include "base/strings/string16.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// Stub implementations for Identity UI on Android.

void ShowWebIDPermissionInfoBar(
    content::WebContents* web_contents,
    const base::string16& message,
    content::IdentityRequestDialogController::InitialApprovalCallback
        callback) {
  NOTIMPLEMENTED();
}

WebIDSigninWindow* ShowWebIDSigninWindow(
    content::WebContents* initiator_web_contents,
    content::WebContents* idp_web_contents,
    const GURL& idp_signin_url,
    content::IdentityRequestDialogController::IdProviderWindowClosedCallback
        on_done) {
  NOTIMPLEMENTED();
  return nullptr;
}

void CloseWebIDSigninWindow(WebIDSigninWindow* window) {
  NOTIMPLEMENTED();
}
