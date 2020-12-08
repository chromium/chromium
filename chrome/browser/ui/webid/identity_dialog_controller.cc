// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webid/identity_dialogs.h"
#include "components/infobars/core/infobar.h"
#include "url/gurl.h"

IdentityDialogController::IdentityDialogController() = default;

IdentityDialogController::~IdentityDialogController() = default;

void IdentityDialogController::ShowInitialPermissionDialog(
    content::WebContents* web_contents,
    InitialApprovalCallback callback) {
  // The WebContents should be that of RP page to make sure info bar is shown on
  // the RP page.

  // TODO(majidvp): Consider using a modal dialog instead of an Inforbar.
  // http://crbug.com/1141125

  // TODO(majidvp): Use a localized string. http://crbug.com/1141125
  ShowWebIDPermissionInfoBar(
      web_contents,
      base::ASCIIToUTF16(
          "WebID: Allow Identity provider to learn about this site?"),
      std::move(callback));
}

void IdentityDialogController::ShowIdProviderWindow(
    content::WebContents* web_contents,
    const GURL& idp_signin_url,
    IdProviderWindowClosedCallback callback) {
  // TODO(majidvp): Pass in a callback to receive the token.
  // http://crbug.com/1141125
  ShowWebIDSigninWindow(web_contents, idp_signin_url,
                        base::OnceCallback<void(std::string)>(),
                        std::move(callback));
}

void IdentityDialogController::ShowTokenExchangePermissionDialog(
    TokenExchangeApprovalCallback callback) {
  // TODO(kenrb): Add Identity permission dialog.
  std::move(callback).Run(
      content::IdentityRequestDialogController::UserApproval::kApproved);
}
