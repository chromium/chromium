// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>

#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webid/account_selection_view.h"

IdentityDialogController::IdentityDialogController(
    content::WebContents* rp_web_contents)
    : rp_web_contents_(rp_web_contents) {}

IdentityDialogController::~IdentityDialogController() = default;

int IdentityDialogController::GetBrandIconMinimumSize() {
  return AccountSelectionView::GetBrandIconMinimumSize();
}

int IdentityDialogController::GetBrandIconIdealSize() {
  return AccountSelectionView::GetBrandIconIdealSize();
}

void IdentityDialogController::ShowAccountsDialog(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::vector<content::IdentityProviderData>& identity_provider_data,
    content::IdentityRequestAccount::SignInMode sign_in_mode,
    bool show_auto_reauthn_checkbox,
    AccountSelectionCallback on_selected,
    DismissCallback dismiss_callback) {
  on_account_selection_ = std::move(on_selected);
  on_dismiss_ = std::move(dismiss_callback);
  if (!account_view_)
    account_view_ = AccountSelectionView::Create(this);
  account_view_->Show(top_frame_for_display, iframe_for_display,
                      identity_provider_data, sign_in_mode,
                      show_auto_reauthn_checkbox);
}

void IdentityDialogController::ShowFailureDialog(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::string& idp_for_display,
    const blink::mojom::RpContext& rp_context,
    const content::IdentityProviderMetadata& idp_metadata,
    DismissCallback dismiss_callback,
    SigninToIdPCallback signin_callback) {
  const GURL rp_url = rp_web_contents_->GetLastCommittedURL();
  on_dismiss_ = std::move(dismiss_callback);
  on_signin_ = std::move(signin_callback);
  if (!account_view_)
    account_view_ = AccountSelectionView::Create(this);
  // Else:
  //   TODO: If the failure dialog is already being shown, notify user that
  //   sign-in attempt failed.

  account_view_->ShowFailureDialog(top_frame_for_display, iframe_for_display,
                                   idp_for_display, rp_context, idp_metadata);
}

void IdentityDialogController::ShowErrorDialog(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::string& idp_for_display,
    const blink::mojom::RpContext& rp_context,
    const content::IdentityProviderMetadata& idp_metadata,
    const absl::optional<TokenError>& error,
    DismissCallback dismiss_callback,
    MoreDetailsCallback more_details_callback) {
  on_dismiss_ = std::move(dismiss_callback);
  on_more_details_ = std::move(more_details_callback);
  if (!account_view_) {
    account_view_ = AccountSelectionView::Create(this);
  }

  account_view_->ShowErrorDialog(top_frame_for_display, iframe_for_display,
                                 idp_for_display, rp_context, idp_metadata,
                                 error);
}

void IdentityDialogController::OnSigninToIdP() {
  std::move(on_signin_).Run();
}

void IdentityDialogController::OnMoreDetails() {
  std::move(on_more_details_).Run();
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

content::WebContents* IdentityDialogController::ShowModalDialog(
    const GURL& url,
    DismissCallback dismiss_callback) {
  // TODO(crbug.com/1429083): connect the dimiss_callback to the
  // modal dialog close button.
  if (!account_view_) {
    account_view_ = AccountSelectionView::Create(this);
  }

  return account_view_->ShowModalDialog(url);
}

void IdentityDialogController::CloseModalDialog() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, this method is invoked on the modal dialog controller,
  // which means we may need to initialize the |account_view|.
  if (!account_view_) {
    account_view_ = AccountSelectionView::Create(this);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  CHECK(account_view_);
  account_view_->CloseModalDialog();
}
