// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_

#include "base/callback.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"

class GURL;
class WebIdDialog;

using UserApproval = content::IdentityRequestDialogController::UserApproval;
using InitialApprovalCallback =
    content::IdentityRequestDialogController::InitialApprovalCallback;
using IdProviderWindowClosedCallback =
    content::IdentityRequestDialogController::IdProviderWindowClosedCallback;
using TokenExchangeApprovalCallback =
    content::IdentityRequestDialogController::TokenExchangeApprovalCallback;

// The IdentityDialogController controls the views that are used across
// browser-mediated federated sign-in flows.
class IdentityDialogController
    : public content::IdentityRequestDialogController {
 public:
  IdentityDialogController();

  IdentityDialogController(const IdentityDialogController&) = delete;
  IdentityDialogController& operator=(const IdentityDialogController&) = delete;

  ~IdentityDialogController() override;

  // content::IdentityRequestDelegate
  void ShowInitialPermissionDialog(content::WebContents* rp_web_contents,
                                   const GURL& idp_url,
                                   InitialApprovalCallback) override;

  void ShowAccountsDialog(content::WebContents* rp_web_contents,
                          content::WebContents* idp_web_contents,
                          const GURL& idp_signin_url,
                          AccountList accounts,
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

 private:
  WebIdDialog& GetOrCreateView(content::WebContents* rp_web_contents);
  WebIdDialog* view_{nullptr};
};

#endif  // CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
