// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_ACCOUNT_SELECTION_VIEW_H_
#define CHROME_BROWSER_UI_WEBID_ACCOUNT_SELECTION_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "ui/gfx/native_ui_types.h"

using Account = content::IdentityRequestAccount;
using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using LinkType = content::IdentityRequestDialogController::LinkType;
using TokenError = content::IdentityCredentialTokenError;

// This class represents the interface used for communicating between the
// identity dialog controller with the Android frontend.
class AccountSelectionView {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Informs the controller that the user has made a selection.
    virtual void OnAccountSelected(
        const GURL& idp_config_url,
        const std::string& account_id,
        const content::IdentityRequestAccount::LoginState& login_state) = 0;
    // TODO(https://crbug.com/377803489): Rename this method to
    // OnUserCancelled.
    // This method is called when the user interacts with the UI in a way that
    // cancels out of the entire fedcm flow.
    virtual void OnDismiss(
        content::IdentityRequestDialogController::DismissReason
            dismiss_reason) = 0;
    virtual void OnLoginToIdP(const GURL& idp_config_url,
                              const GURL& idp_login_url) = 0;
    virtual void OnMoreDetails() = 0;
    // Informs the controller that the accounts dialog has been displayed.
    virtual void OnAccountsDisplayed() = 0;
    // The web page view containing the focused field.
    virtual gfx::NativeView GetNativeView() = 0;
    // The WebContents for the page.
    virtual content::WebContents* GetWebContents() = 0;
  };

  static std::unique_ptr<AccountSelectionView> Create(Delegate* delegate);

  // Returns the brand icon minimum size. This includes the size of the
  // safe-zone defined in https://www.w3.org/TR/appmanifest/#icon-masks
  static int GetBrandIconMinimumSize(blink::mojom::RpMode rp_mode);

  // Returns the brand icon ideal size. This includes the size of the
  // safe-zone defined in https://www.w3.org/TR/appmanifest/#icon-masks
  static int GetBrandIconIdealSize(blink::mojom::RpMode rp_mode);

  explicit AccountSelectionView(Delegate* delegate) : delegate_(delegate) {}
  AccountSelectionView(const AccountSelectionView&) = delete;
  AccountSelectionView& operator=(const AccountSelectionView&) = delete;
  virtual ~AccountSelectionView() = default;

  // Instructs the view to show the provided accounts to the user.
  // `rp_data` is the relying party's data, such as the display name and icon.
  // All IDP-specific information, is stored in `idp_list`. `sign_in_mode`
  // represents whether this is an auto re-authn flow. If it is the auto
  // re-authn flow, `idp_list` will only include the single returning account
  // and its IDP. `new_accounts` is a vector where each member is a newly logged
  // in account that ought to be prioritized in the UI. Returns true if it was
  // possible to show UI. If this method could not show UI and called Dismiss,
  // returns false.
  virtual bool Show(
      const content::RelyingPartyData& rp_data,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      blink::mojom::RpMode rp_mode,
      const std::vector<IdentityRequestAccountPtr>& new_accounts) = 0;

  // Shows a failure UI when the accounts fetch is failed such that it is
  // observable by users. This could happen when an IDP claims that the user is
  // signed in but not respond with any user account during browser fetches.
  // Returns true if it was possible to show UI. If this method could not show
  // UI and called Dismiss, returns false.
  virtual bool ShowFailureDialog(
      const content::RelyingPartyData& rp_data,
      const std::string& idp_for_display,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const content::IdentityProviderMetadata& idp_metadata) = 0;

  // Shows an error dialog to the user, possibly with a custom error message.
  // Returns true if it was possible to show UI. If this method could not show
  // UI and called Dismiss, returns false.
  virtual bool ShowErrorDialog(
      const content::RelyingPartyData& rp_data,
      const std::string& idp_for_display,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const content::IdentityProviderMetadata& idp_metadata,
      const std::optional<TokenError>& error) = 0;

  // Shows a loading dialog to the user. Used in the button mode, to acknowledge
  // the user interaction. Returns true if it was possible to show UI. If this
  // method could not show UI and called Dismiss, returns false.
  virtual bool ShowLoadingDialog(const content::RelyingPartyData& rp_data,
                                 const std::string& idp_for_display,
                                 blink::mojom::RpContext rp_context,
                                 blink::mojom::RpMode rp_mode) = 0;

  // Shows a verifying dialog to the user. This is called after an account is
  // selected, either by the user in the explicit authentication flow or by the
  // browser in the auto re-authentication flow. Returns true if it was possible
  // to show UI.
  virtual bool ShowVerifyingDialog(const content::RelyingPartyData& rp_data,
                                   const IdentityProviderDataPtr& idp_data,
                                   const IdentityRequestAccountPtr& account,
                                   Account::SignInMode sign_in_mode,
                                   blink::mojom::RpMode rp_mode) = 0;

  virtual std::string GetTitle() const = 0;
  virtual std::optional<std::string> GetSubtitle() const = 0;

  virtual void ShowUrl(LinkType type, const GURL& url) = 0;
  virtual content::WebContents* ShowModalDialog(
      const GURL& url,
      blink::mojom::RpMode rp_mode) = 0;
  virtual void CloseModalDialog() = 0;
  virtual content::WebContents* GetRpWebContents() = 0;

 protected:
  raw_ptr<Delegate> delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBID_ACCOUNT_SELECTION_VIEW_H_
