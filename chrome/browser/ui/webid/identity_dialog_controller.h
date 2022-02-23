// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class WebIdDialog;

using UserApproval = content::IdentityRequestDialogController::UserApproval;
using InitialApprovalCallback =
    content::IdentityRequestDialogController::InitialApprovalCallback;
using IdProviderWindowClosedCallback =
    content::IdentityRequestDialogController::IdProviderWindowClosedCallback;
using TokenExchangeApprovalCallback =
    content::IdentityRequestDialogController::TokenExchangeApprovalCallback;
using AccountSelectionCallback =
    content::IdentityRequestDialogController::AccountSelectionCallback;

// The IdentityDialogController controls the views that are used across
// browser-mediated federated sign-in flows.
class IdentityDialogController
    : public content::IdentityRequestDialogController,
      public AccountSelectionView::Delegate {
 public:
  IdentityDialogController();

  IdentityDialogController(const IdentityDialogController&) = delete;
  IdentityDialogController& operator=(const IdentityDialogController&) = delete;

  ~IdentityDialogController() override;

  // content::IdentityRequestDelegate
  int GetBrandIconMinimumSize() override;
  int GetBrandIconIdealSize() override;

  void ShowInitialPermissionDialog(
      content::WebContents* rp_web_contents,
      const GURL& idp_url,
      content::IdentityRequestDialogController::PermissionDialogMode mode,
      InitialApprovalCallback) override;

  void ShowAccountsDialog(
      content::WebContents* rp_web_contents,
      content::WebContents* idp_web_contents,
      const GURL& idp_url,
      base::span<const content::IdentityRequestAccount> accounts,
      const content::IdentityProviderMetadata& idp_metadata,
      const content::ClientIdData& client_data,
      content::IdentityRequestAccount::SignInMode sign_in_mode,
      AccountSelectionCallback on_selected) override;

  void ShowIdProviderWindow(content::WebContents* rp_web_contents,
                            content::WebContents* idp_web_contents,
                            const GURL& idp_signin_url,
                            IdProviderWindowClosedCallback) override;

  void CloseIdProviderWindow() override;

  void ShowTokenExchangePermissionDialog(
      content::WebContents* rp_web_contents,
      const GURL& idp_url,
      TokenExchangeApprovalCallback) override;

  // AccountSelectionView::Delegate:

  void OnAccountSelected(const Account& account) override;
  void OnDismiss() override;

  // The web page view containing the focused field.
  gfx::NativeView GetNativeView() override;

 private:
  WebIdDialog& GetOrCreateView(content::WebContents* rp_web_contents);
  raw_ptr<WebIdDialog> view_{nullptr};

  void OnViewClosed();

  std::unique_ptr<AccountSelectionView> account_view_{nullptr};
  AccountSelectionCallback on_account_selection_;
  raw_ptr<content::WebContents> rp_web_contents_;
  IdProviderWindowClosedCallback view_closed_callback_;

  base::WeakPtrFactory<IdentityDialogController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
