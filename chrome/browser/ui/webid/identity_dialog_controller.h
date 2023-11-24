// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"

using AccountSelectionCallback =
    content::IdentityRequestDialogController::AccountSelectionCallback;
using DismissCallback =
    content::IdentityRequestDialogController::DismissCallback;
using TokenError = content::IdentityCredentialTokenError;

// The IdentityDialogController controls the views that are used across
// browser-mediated federated sign-in flows.
class IdentityDialogController
    : public content::IdentityRequestDialogController,
      public AccountSelectionView::Delegate {
 public:
  explicit IdentityDialogController(content::WebContents* rp_web_contents);

  IdentityDialogController(const IdentityDialogController&) = delete;
  IdentityDialogController& operator=(const IdentityDialogController&) = delete;

  ~IdentityDialogController() override;

  // content::IdentityRequestDelegate
  int GetBrandIconMinimumSize() override;
  int GetBrandIconIdealSize() override;

  // content::IdentityRequestDialogController
  void ShowAccountsDialog(
      const std::string& top_frame_for_display,
      const absl::optional<std::string>& iframe_for_display,
      const std::vector<content::IdentityProviderData>& identity_provider_data,
      content::IdentityRequestAccount::SignInMode sign_in_mode,
      bool show_auto_reauthn_checkbox,
      AccountSelectionCallback on_selected,
      LoginToIdPCallback on_add_account,
      DismissCallback dismiss_callback) override;
  void ShowFailureDialog(const std::string& top_frame_for_display,
                         const absl::optional<std::string>& iframe_for_display,
                         const std::string& idp_for_display,
                         const blink::mojom::RpContext& rp_context,
                         const content::IdentityProviderMetadata& idp_metadata,
                         DismissCallback dismiss_callback,
                         LoginToIdPCallback login_callback) override;
  void ShowErrorDialog(const std::string& top_frame_for_display,
                       const absl::optional<std::string>& iframe_for_display,
                       const std::string& idp_for_display,
                       const blink::mojom::RpContext& rp_context,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const absl::optional<TokenError>& error,
                       DismissCallback dismiss_callback,
                       MoreDetailsCallback more_details_callback) override;
  void ShowIdpSigninFailureDialog(base::OnceClosure dismiss_callback) override;

  std::string GetTitle() const override;
  absl::optional<std::string> GetSubtitle() const override;

  // Show a modal dialog that loads content from the IdP in a WebView.
  content::WebContents* ShowModalDialog(
      const GURL& url,
      DismissCallback dismiss_callback) override;
  void CloseModalDialog() override;

  // AccountSelectionView::Delegate:
  void OnAccountSelected(const GURL& idp_config_url,
                         const Account& account) override;
  void OnDismiss(DismissReason dismiss_reason) override;
  void OnLoginToIdP(const GURL& idp_login_url) override;
  void OnMoreDetails() override;
  gfx::NativeView GetNativeView() override;
  content::WebContents* GetWebContents() override;

 private:
  std::unique_ptr<AccountSelectionView> account_view_{nullptr};
  AccountSelectionCallback on_account_selection_;
  DismissCallback on_dismiss_;
  LoginToIdPCallback on_login_;
  MoreDetailsCallback on_more_details_;
  raw_ptr<content::WebContents> rp_web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
