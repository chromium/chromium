// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/browser/webid/identity_provider_permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-shared.h"

IdentityDialogController::IdentityDialogController(
    content::WebContents* rp_web_contents)
    : rp_web_contents_(rp_web_contents) {}

IdentityDialogController::~IdentityDialogController() = default;

int IdentityDialogController::GetBrandIconMinimumSize(
    blink::mojom::RpMode rp_mode) {
  return AccountSelectionView::GetBrandIconMinimumSize(rp_mode);
}

int IdentityDialogController::GetBrandIconIdealSize(
    blink::mojom::RpMode rp_mode) {
  return AccountSelectionView::GetBrandIconIdealSize(rp_mode);
}

bool IdentityDialogController::ShowAccountsDialog(
    const std::string& rp_for_display,
    const std::vector<content::IdentityProviderData>& identity_provider_data,
    content::IdentityRequestAccount::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode,
    const std::optional<content::IdentityProviderData>& new_account_idp,
    AccountSelectionCallback on_selected,
    LoginToIdPCallback on_add_account,
    DismissCallback dismiss_callback,
    AccountsDisplayedCallback accounts_displayed_callback) {
  on_account_selection_ = std::move(on_selected);
  on_login_ = std::move(on_add_account);
  on_dismiss_ = std::move(dismiss_callback);
  on_accounts_displayed_ = std::move(accounts_displayed_callback);
  rp_mode_ = rp_mode;
  if (!account_view_)
    account_view_ = AccountSelectionView::Create(this);
  return account_view_->Show(rp_for_display, identity_provider_data,
                             sign_in_mode, rp_mode, new_account_idp);
}

bool IdentityDialogController::ShowFailureDialog(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata,
    DismissCallback dismiss_callback,
    LoginToIdPCallback login_callback) {
  const GURL rp_url = rp_web_contents_->GetLastCommittedURL();
  on_dismiss_ = std::move(dismiss_callback);
  on_login_ = std::move(login_callback);
  if (!account_view_)
    account_view_ = AccountSelectionView::Create(this);
  // Else:
  //   TODO: If the failure dialog is already being shown, notify user that
  //   sign-in attempt failed.

  return account_view_->ShowFailureDialog(rp_for_display, idp_for_display,
                                          rp_context, rp_mode, idp_metadata);
}

bool IdentityDialogController::ShowErrorDialog(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error,
    DismissCallback dismiss_callback,
    MoreDetailsCallback more_details_callback) {
  on_dismiss_ = std::move(dismiss_callback);
  on_more_details_ = std::move(more_details_callback);
  if (!account_view_) {
    account_view_ = AccountSelectionView::Create(this);
  }

  return account_view_->ShowErrorDialog(rp_for_display, idp_for_display,
                                        rp_context, rp_mode, idp_metadata,
                                        error);
}

bool IdentityDialogController::ShowLoadingDialog(
    const std::string& rp_for_display,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    DismissCallback dismiss_callback) {
  on_dismiss_ = std::move(dismiss_callback);
  if (!account_view_) {
    account_view_ = AccountSelectionView::Create(this);
  }

  return account_view_->ShowLoadingDialog(rp_for_display, idp_for_display,
                                          rp_context, rp_mode);
}

void IdentityDialogController::OnLoginToIdP(const GURL& idp_config_url,
                                            const GURL& idp_login_url) {
  CHECK(on_login_);
  on_login_.Run(idp_config_url, idp_login_url);
}

void IdentityDialogController::OnMoreDetails() {
  CHECK(on_more_details_);
  std::move(on_more_details_).Run();
}

void IdentityDialogController::OnAccountsDisplayed() {
  CHECK(on_accounts_displayed_);
  std::move(on_accounts_displayed_).Run();
}

void IdentityDialogController::OnAccountSelected(const GURL& idp_config_url,
                                                 const Account& account) {
  CHECK(on_account_selection_);

  // We only allow dismiss after account selection on button flows and not on
  // widget flows.
  // TODO(crbug.com/335886093): Figure out whether users can cancel after
  // selecting an account on button flow modal.
  if (rp_mode_ == blink::mojom::RpMode::kWidget) {
    on_dismiss_.Reset();
  }

  std::move(on_account_selection_)
      .Run(idp_config_url, account.id,
           account.login_state ==
               content::IdentityRequestAccount::LoginState::kSignIn);
}

void IdentityDialogController::OnDismiss(DismissReason dismiss_reason) {
  // |OnDismiss| can be called after |OnAccountSelected| which sets the callback
  // to null.
  if (!on_dismiss_) {
    return;
  }

  on_account_selection_.Reset();
  std::move(on_dismiss_).Run(dismiss_reason);
}

std::string IdentityDialogController::GetTitle() const {
  return account_view_->GetTitle();
}

std::optional<std::string> IdentityDialogController::GetSubtitle() const {
  return account_view_->GetSubtitle();
}

gfx::NativeView IdentityDialogController::GetNativeView() {
  return rp_web_contents_->GetNativeView();
}

content::WebContents* IdentityDialogController::GetWebContents() {
  return rp_web_contents_;
}

void IdentityDialogController::ShowUrl(LinkType type, const GURL& url) {
  if (!account_view_) {
    return;
  }
  account_view_->ShowUrl(type, url);
}

content::WebContents* IdentityDialogController::ShowModalDialog(
    const GURL& url,
    blink::mojom::RpMode rp_mode,
    DismissCallback dismiss_callback) {
  on_dismiss_ = std::move(dismiss_callback);
  if (!account_view_) {
    account_view_ = AccountSelectionView::Create(this);
  }

  return account_view_->ShowModalDialog(url, rp_mode);
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

content::WebContents* IdentityDialogController::GetRpWebContents() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, this method is invoked on the modal dialog controller,
  // which means we may need to initialize the |account_view|.
  if (!account_view_) {
    account_view_ = AccountSelectionView::Create(this);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  CHECK(account_view_);
  return account_view_->GetRpWebContents();
}

void IdentityDialogController::RequestIdPRegistrationPermision(
    const url::Origin& origin,
    base::OnceCallback<void(bool accepted)> callback) {
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(rp_web_contents_);

  auto* request =
      new IdentityProviderPermissionRequest(origin, std::move(callback));

  permission_request_manager->AddRequest(
      rp_web_contents_->GetPrimaryMainFrame(), request);
}

void IdentityDialogController::SetAccountSelectionViewForTesting(
    std::unique_ptr<AccountSelectionView> account_view) {
  account_view_ = std::move(account_view);
}
