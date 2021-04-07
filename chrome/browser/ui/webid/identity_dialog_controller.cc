// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webid/webid_dialog.h"
#include "components/infobars/core/infobar.h"
#include "url/gurl.h"

IdentityDialogController::IdentityDialogController() = default;

IdentityDialogController::~IdentityDialogController() = default;

void IdentityDialogController::ShowInitialPermissionDialog(
    content::WebContents* rp_web_contents,
    const GURL& idp_url,
    InitialApprovalCallback callback) {
  DCHECK(!view_);

  // The WebContents should be that of RP page to make sure info bar is shown on
  // the RP page.

  // TODO(majidvp): Use the provider name/url here
  auto idp_hostname = base::UTF8ToUTF16(idp_url.GetOrigin().host());

  auto rp_hostname =
      base::UTF8ToUTF16(rp_web_contents->GetVisibleURL().GetOrigin().host());

  GetOrCreateView(rp_web_contents)
      .ShowInitialPermission(idp_hostname, rp_hostname, std::move(callback));
}

void IdentityDialogController::ShowIdProviderWindow(
    content::WebContents* rp_web_contents,
    content::WebContents* idp_web_contents,
    const GURL& idp_signin_url,
    IdProviderWindowClosedCallback callback) {
  GetOrCreateView(rp_web_contents)
      .ShowSigninPage(idp_web_contents, idp_signin_url, std::move(callback));
}

void IdentityDialogController::CloseIdProviderWindow() {
  // TODO(majidvp): This may race with user closing the signin window directly.
  // So we should not really check the signin_window_ instead we should setup
  // the on_close callback here here and check that to avoid lifetime issues.
  if (!view_)
    return;

  // Note that this leads to the window closed callback being run. If the
  // token exchange permission dialog does not need to be displayed, the
  // identity request will be completed synchronously and this controller will
  // be destroyed.
  // TODO(kenrb, majidvp): Not knowing whether this object will be destroyed
  // or not during the callback is problematic. We have to rethink the
  // lifetimes.
  view_->CloseSigninPage();

  // Do not touch local state here since |this| is now destroyed.
}

void IdentityDialogController::ShowTokenExchangePermissionDialog(
    content::WebContents* rp_web_contents,
    const GURL& idp_url,
    TokenExchangeApprovalCallback callback) {
  auto idp_hostname = base::UTF8ToUTF16(idp_url.GetOrigin().host());

  auto rp_hostname =
      base::UTF8ToUTF16(rp_web_contents->GetVisibleURL().GetOrigin().host());

  GetOrCreateView(rp_web_contents)
      .ShowTokenExchangePermission(idp_hostname, rp_hostname,
                                   std::move(callback));
}

WebIdDialog& IdentityDialogController::GetOrCreateView(
    content::WebContents* rp_web_contents) {
  if (!view_)
    view_ = WebIdDialog::Create(rp_web_contents);

  // It is expected that we use the same rp_web_contents during the lifetime
  // of this controller.
  DCHECK_EQ(view_->rp_web_contents(), rp_web_contents);

  return *view_;
}

void IdentityDialogController::ShowAccountsDialog(
    content::WebContents* rp_web_contents,
    content::WebContents* idp_web_contents,
    const GURL& idp_signin_url,
    AccountList accounts,
    AccountSelectionCallback on_selected) {
  std::move(on_selected).Run(accounts[0].sub);
}
