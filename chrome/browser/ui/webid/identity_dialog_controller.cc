// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include "url/gurl.h"

IdentityDialogController::IdentityDialogController() = default;

IdentityDialogController::~IdentityDialogController() = default;

void IdentityDialogController::ShowInitialPermissionDialog(
    InitialApprovalCallback callback) {
  // TODO(kenrb): Add UI permission dialog.
  std::move(callback).Run(
      content::IdentityRequestDialogController::UserApproval::kApproved);
}

void IdentityDialogController::ShowIdProviderWindow(
    const GURL& idp_signin_url,
    IdProviderWindowClosedCallback callback) {
  // TODO(kenrb): Add UI modal window to display the IDP UI and load the IDP
  // page.
  std::move(callback).Run();
}

void IdentityDialogController::ShowTokenExchangePermissionDialog(
    TokenExchangeApprovalCallback callback) {
  // TODO(kenrb): Add Identity permission dialog.
  std::move(callback).Run(
      content::IdentityRequestDialogController::UserApproval::kApproved);
}
