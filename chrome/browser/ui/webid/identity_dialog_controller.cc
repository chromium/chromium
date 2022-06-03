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
#include "components/url_formatter/elide_url.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

std::string FormatUrlForDisplay(const GURL& url) {
  std::string formatted_url_str =
      net::IsLocalhost(url)
          ? url.GetWithEmptyPath().spec()
          : net::registry_controlled_domains::GetDomainAndRegistry(
                url,
                net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
      GURL(url.scheme() + "://" + formatted_url_str),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
}

}  // namespace

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
  std::string rp_for_display =
      FormatUrlForDisplay(rp_web_contents_->GetLastCommittedURL());
  std::string idp_for_display = FormatUrlForDisplay(idp_url);

  if (!account_view_)
    account_view_ = AccountSelectionView::Create(this);
  account_view_->Show(rp_for_display, idp_for_display, accounts, idp_metadata,
                      client_data, sign_in_mode);
}

void IdentityDialogController::OnAccountSelected(const Account& account) {
  std::move(on_account_selection_)
      .Run(account.id,
           account.login_state ==
               content::IdentityRequestAccount::LoginState::kSignIn,
           /* should_embargo=*/false);
}

void IdentityDialogController::OnDismiss(bool should_embargo) {
  // |OnDismiss| can be called after |OnAccountSelected| which sets the callback
  // to null.
  if (on_account_selection_)
    std::move(on_account_selection_).Run(std::string(), false, should_embargo);
}

gfx::NativeView IdentityDialogController::GetNativeView() {
  return rp_web_contents_->GetNativeView();
}

content::WebContents* IdentityDialogController::GetWebContents() {
  return rp_web_contents_;
  ;
}
