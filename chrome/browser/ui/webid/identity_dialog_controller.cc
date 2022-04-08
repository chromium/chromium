// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "components/infobars/core/infobar.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

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
    const GURL& idp_url,
    base::span<const content::IdentityRequestAccount> accounts,
    const content::IdentityProviderMetadata& idp_metadata,
    const content::ClientIdData& client_data,
    content::IdentityRequestAccount::SignInMode sign_in_mode,
    AccountSelectionCallback on_selected) {
  // IDP scheme is expected to always be `https://`.
  CHECK(idp_url.SchemeIs(url::kHttpsScheme));
  rp_web_contents_ = rp_web_contents;
  on_account_selection_ = std::move(on_selected);
  std::string rp_etld_plus_one =
      net::registry_controlled_domains::GetDomainAndRegistry(
          rp_web_contents_->GetLastCommittedURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  std::string idp_etld_plus_one =
      net::registry_controlled_domains::GetDomainAndRegistry(
          idp_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (!account_view_)
    account_view_ = AccountSelectionView::Create(this);
  account_view_->Show(rp_etld_plus_one, idp_etld_plus_one, accounts,
                      idp_metadata, client_data, sign_in_mode);
}

void IdentityDialogController::OnAccountSelected(const Account& account) {
  std::move(on_account_selection_)
      .Run(account.id,
           account.login_state ==
               content::IdentityRequestAccount::LoginState::kSignIn);
}

void IdentityDialogController::OnDismiss() {
  // |OnDismiss| can be called after |OnAccountSelected| which sets the callback
  // to null.
  if (on_account_selection_)
    std::move(on_account_selection_).Run(std::string(), false);
}

gfx::NativeView IdentityDialogController::GetNativeView() {
  return rp_web_contents_->GetNativeView();
}

content::WebContents* IdentityDialogController::GetWebContents() {
  return rp_web_contents_;
  ;
}
