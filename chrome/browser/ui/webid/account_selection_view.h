// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_ACCOUNT_SELECTION_VIEW_H_
#define CHROME_BROWSER_UI_WEBID_ACCOUNT_SELECTION_VIEW_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "ui/gfx/native_widget_types.h"

using Account = content::IdentityRequestAccount;
using TokenError = content::IdentityCredentialTokenError;

// This class represents the interface used for communicating between the
// identity dialog controller with the Android frontend.
class AccountSelectionView {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Informs the controller that the user has made a selection.
    virtual void OnAccountSelected(const GURL& idp_config_url,
                                   const Account& account) = 0;
    // Informs the controller that the user has dismissed the sheet with reason
    // `dismiss_reason`.
    virtual void OnDismiss(
        content::IdentityRequestDialogController::DismissReason
            dismiss_reason) = 0;
    virtual void OnSigninToIdP() = 0;
    // The web page view containing the focused field.
    virtual gfx::NativeView GetNativeView() = 0;
    // The WebContents for the page.
    virtual content::WebContents* GetWebContents() = 0;
  };

  static std::unique_ptr<AccountSelectionView> Create(Delegate* delegate);

  // Returns the brand icon minimum size. This includes the size of the
  // safe-zone defined in https://www.w3.org/TR/appmanifest/#icon-masks
  static int GetBrandIconMinimumSize();

  // Returns the brand icon ideal size. This includes the size of the
  // safe-zone defined in https://www.w3.org/TR/appmanifest/#icon-masks
  static int GetBrandIconIdealSize();

  explicit AccountSelectionView(Delegate* delegate) : delegate_(delegate) {}
  AccountSelectionView(const AccountSelectionView&) = delete;
  AccountSelectionView& operator=(const AccountSelectionView&) = delete;
  virtual ~AccountSelectionView() = default;

  // Instructs the view to show the provided accounts to the user.
  // `top_frame_for_display` is the relying party's top frame URL and
  // `iframe_for_display` is the relying party's iframe URL to display in
  // the prompt. All IDP-specific information, including user accounts, is
  // stored in `idps_for_display`. `sign_in_mode` represents whether this is an
  // auto re-authn flow. If it is the auto re-authn flow, `idps_for_display`
  // will only include the single returning account and its IDP.
  // `show_auto_reauthn_checkbox` represents whether we should show a checkbox
  // for users to opt out of auto re-authn. After user interaction either
  // OnAccountSelected() or OnDismiss() gets invoked.
  virtual void Show(
      const std::string& top_frame_for_display,
      const absl::optional<std::string>& iframe_for_display,
      const std::vector<content::IdentityProviderData>& identity_provider_data,
      Account::SignInMode sign_in_mode,
      bool show_auto_reauthn_checkbox) = 0;

  // Shows a failure UI when the accounts fetch is failed such that it is
  // observable by users. This could happen when an IDP claims that the user is
  // signed in but not respond with any user account during browser fetches.
  virtual void ShowFailureDialog(
      const std::string& top_frame_for_display,
      const absl::optional<std::string>& iframe_for_display,
      const std::string& idp_for_display,
      const blink::mojom::RpContext& rp_context,
      const content::IdentityProviderMetadata& idp_metadata) = 0;

  virtual void ShowErrorDialog(
      const std::string& top_frame_for_display,
      const absl::optional<std::string>& iframe_for_display,
      const std::string& idp_for_display,
      const blink::mojom::RpContext& rp_context,
      const content::IdentityProviderMetadata& idp_metadata,
      const absl::optional<TokenError>& error) = 0;

  virtual std::string GetTitle() const = 0;
  virtual absl::optional<std::string> GetSubtitle() const = 0;

  virtual content::WebContents* ShowModalDialog(const GURL& url) = 0;
  virtual void CloseModalDialog() = 0;

 protected:
  raw_ptr<Delegate> delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBID_ACCOUNT_SELECTION_VIEW_H_
