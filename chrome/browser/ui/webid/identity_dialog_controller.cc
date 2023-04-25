// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>

#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webid/account_selection_view.h"

IdentityDialogController::IdentityDialogController() = default;

IdentityDialogController::~IdentityDialogController() = default;

int IdentityDialogController::GetBrandIconMinimumSize() {
  return AccountSelectionView::GetBrandIconMinimumSize();
}

int IdentityDialogController::GetBrandIconIdealSize() {
  return AccountSelectionView::GetBrandIconIdealSize();
}

void IdentityDialogController::ShowAccountsDialog(
    content::WebContents* rp_web_contents,
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::vector<content::IdentityProviderData>& identity_provider_data,
    content::IdentityRequestAccount::SignInMode sign_in_mode,
    bool show_auto_reauthn_checkbox,
    AccountSelectionCallback on_selected,
    DismissCallback dismiss_callback) {
  rp_web_contents_ = rp_web_contents;
  on_account_selection_ = std::move(on_selected);
  on_dismiss_ = std::move(dismiss_callback);
  if (!account_view_)
    account_view_ = AccountSelectionView::Create(this);
  account_view_->Show(top_frame_for_display, iframe_for_display,
                      identity_provider_data, sign_in_mode,
                      show_auto_reauthn_checkbox);
}

void IdentityDialogController::ShowFailureDialog(
    content::WebContents* rp_web_contents,
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::string& idp_for_display,
    const content::IdentityProviderMetadata& idp_metadata,
    DismissCallback dismiss_callback) {
  const GURL rp_url = rp_web_contents->GetLastCommittedURL();
  rp_web_contents_ = rp_web_contents;
  on_dismiss_ = std::move(dismiss_callback);
  if (!account_view_)
    account_view_ = AccountSelectionView::Create(this);
  // Else:
  //   TODO: If the failure dialog is already being shown, notify user that
  //   sign-in attempt failed.

  account_view_->ShowFailureDialog(top_frame_for_display, iframe_for_display,
                                   idp_for_display, idp_metadata);
}

void IdentityDialogController::ShowIdpSigninFailureDialog(
    base::OnceClosure user_notified_callback) {
  NOTIMPLEMENTED();
}

std::string IdentityDialogController::GetTitle() const {
  return account_view_->GetTitle();
}

absl::optional<std::string> IdentityDialogController::GetSubtitle() const {
  return account_view_->GetSubtitle();
}

void IdentityDialogController::OnAccountSelected(const GURL& idp_config_url,
                                                 const Account& account) {
  on_dismiss_.Reset();
  std::move(on_account_selection_)
      .Run(idp_config_url, account.id,
           account.login_state ==
               content::IdentityRequestAccount::LoginState::kSignIn);
}

void IdentityDialogController::OnDismiss(DismissReason dismiss_reason) {
  // |OnDismiss| can be called after |OnAccountSelected| which sets the callback
  // to null.
  if (on_dismiss_) {
    on_account_selection_.Reset();
    std::move(on_dismiss_).Run(dismiss_reason);
  }
}

gfx::NativeView IdentityDialogController::GetNativeView() {
  return rp_web_contents_->GetNativeView();
}

content::WebContents* IdentityDialogController::GetWebContents() {
  return rp_web_contents_;
}
