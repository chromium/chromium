// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOGS_H_
#define CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOGS_H_

#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "url/gurl.h"

class WebIDSigninWindow;

namespace content {
class WebContents;
}  // namespace content

// Creates and shows a confirmation infobar and delegate and adds the infobar to
// |infobar_service|. The provided callback is called with appropriate status
// depending on whether user accepted or denied/closed the infobar.
void ShowWebIDPermissionInfoBar(
    content::WebContents* web_contents,
    const base::string16& message,
    content::IdentityRequestDialogController::InitialApprovalCallback callback);

// Creates and shows a window that loads the identity provider sign in page at
// the given URL. The provided callback is called when IDP has provided an
// id_token with the id_token a its argument, or when window is closed by user
// with an empty string as its argument.
WebIDSigninWindow* ShowWebIDSigninWindow(
    content::WebContents* initiator_web_contents,
    content::WebContents* idp_web_contents,
    const GURL& idp_signin_url,
    content::IdentityRequestDialogController::IdProviderWindowClosedCallback
        on_done);

void CloseWebIDSigninWindow(WebIDSigninWindow* window);

#endif  // CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOGS_H_
