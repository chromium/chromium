// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_VIEW_CONTROLLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/signin/signin_modal_dialog.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/base/data_type.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class Browser;
struct AccountInfo;
struct CoreAccountId;

namespace content {
class WebContents;
}

namespace login_ui_test_utils {
class SigninViewControllerTestUtil;
}

namespace signin_metrics {
enum class AccessPoint;
enum class PromoAction;
enum class Reason;
enum class ReauthAccessPoint;
enum class SourceForRefreshTokenOperation;
}  // namespace signin_metrics

namespace signin {
enum class ReauthResult;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
namespace {
class NewTabWebContentsObserver;
}
#endif

// Class responsible for showing and hiding all sign-in related UIs
// (modal sign-in, DICE full-tab sign-in page, sync confirmation dialog, sign-in
// error dialog, reauth prompt). Sync confirmation is used on
// Win/Mac/Linux/Chrome OS. Sign-in is only used on Win/Mac/Linux because
// Chrome OS has its own sign-in flow and doesn't use DICE.
class SigninViewController {
 public:
  // Handle that will stop ongoing reauths upon destruction.
  class ReauthAbortHandle {
   public:
    virtual ~ReauthAbortHandle() = default;
  };

  explicit SigninViewController(Browser* browser);

  SigninViewController(const SigninViewController&) = delete;
  SigninViewController& operator=(const SigninViewController&) = delete;

  virtual ~SigninViewController();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Returns true if Chrome new tab page/blank is displayed in `contents`.
  static bool IsNTPTab(content::WebContents* contents);

  // Shows the signin attached to |browser_|'s active web contents.
  // |access_point| indicates the access point used to open the Gaia sign in
  // page.
  // DEPRECATED: Use ShowDiceEnableSyncTab instead.
  void ShowSignin(signin_metrics::AccessPoint access_point,
                  const GURL& redirect_url = GURL(chrome::kChromeUINewTabURL));

  // Shows a Chrome Sync signin tab. |email_hint| may be empty.
  // Note: If the user has already set a primary account, then this is
  // considered a reauth of the primary account, and |email_hint| is ignored.
  void ShowDiceEnableSyncTab(signin_metrics::AccessPoint access_point,
                             signin_metrics::PromoAction promo_action,
                             const std::string& email_hint);

  // Shows the Dice "add account" tab, which adds an account to the browser but
  // does not turn sync on. |email_hint| may be empty.
  virtual void ShowDiceAddAccountTab(signin_metrics::AccessPoint access_point,
                                     const std::string& email_hint);

  // Opens the Gaia logout page in a new tab. This removes the accounts from the
  // web, as well as from Chrome (Dice intercepts the web signout and
  // invalidates all Chrome accounts). If a primary account is set, this
  // function does not clear it, but still invalidates its credentials.
  // This is the only way to properly signout all accounts. In particular,
  // calling Gaia logout programmatically or revoking the tokens does not sign
  // out SAML accounts completely (see https://crbug.com/1069421).
  void ShowGaiaLogoutTab(signin_metrics::SourceForRefreshTokenOperation source);

  // Shows the modal signin intercept first run experience dialog as a
  // browser-modal dialog on top of the `browser_`'s window. `account_id`
  // corresponds to the intercepted account.
  void ShowModalInterceptFirstRunExperienceDialog(
      const CoreAccountId& account_id,
      bool is_forced_intercept);

  // Possibly show a confirmation prompt and then sign the user out, open a
  // reauth tab, or do nothing depending on the user choice.
  void SignoutOrReauthWithPrompt(
      signin_metrics::AccessPoint reauth_access_point,
      signin_metrics::ProfileSignout profile_signout_source,
      signin_metrics::SourceForRefreshTokenOperation token_signout_source);

  // Called by extensions to ask the user to sign in to chrome while they are
  // signed in on the web only.
  // This opens/reuses a new tab page and opens a modal dialog.
  // Note: This should  only be called if the dialog is not already showing.
  void MaybeShowChromeSigninDialogForExtensions(std::string_view extension_name,
                                                base::OnceClosure on_complete);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Shows the modal profile customization dialog as a browser-modal dialog on
  // top of the |browser_|'s window.
  void ShowModalProfileCustomizationDialog(
      bool is_local_profile_creation = false);

  // Shows the modal sign-in email confirmation dialog as a tab-modal dialog on
  // top of the currently displayed WebContents in |browser_|.
  void ShowModalSigninEmailConfirmationDialog(
      const std::string& last_email,
      const std::string& email,
      SigninEmailConfirmationDialog::Callback callback);

  // Shows the reauth prompt for |account_id| as either:
  // - a tab-modal dialog on top of the currently active tab, or
  // - a new tab
  // |account_id| should be signed into the content area. Otherwise, the method
  // fails with |kAccountNotSignedIn| error.
  // |access_point| indicates a call site of this method.
  // Calls |reauth_callback| on completion of the reauth flow, or on error. The
  // callback may be called synchronously. The user may also ignore the reauth
  // indefinitely.
  // Returns a handle that aborts the ongoing reauth on destruction.
  virtual std::unique_ptr<ReauthAbortHandle> ShowReauthPrompt(
      const CoreAccountId& account_id,
      signin_metrics::ReauthAccessPoint access_point,
      base::OnceCallback<void(signin::ReauthResult)> reauth_callback);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

  // Shows the modal sync confirmation dialog as a browser-modal dialog on top
  // of the |browser_|'s window.
  // `is_signin_intercept` is true if the confirmation dialog is shown after
  // signin intercept, which has a slightly different UI.
  // `is_sync_promo` is true if the sync confirmation dialog is offered as an
  // option. It is false if the user explicitly initiated the flow.
  void ShowModalSyncConfirmationDialog(bool is_signin_intercept,
                                       bool is_sync_promo);

  // Shows the modal managed user notice dialog as a browser-modal dialog on
  // top of the `browser_`'s window. `domain_name` is the domain of the
  // enterprise account being shown. `callback` is called with the user's action
  // on the dialog.
  // If `profile_creation_required_by_policy` is true, the wording of the dialog
  // will tell the user that an admin requires a new profile for the account,
  // otherwise the default wording will be used.
  // When `show_link_data_option` is false, the callback is called with either
  // SIGNIN_CHOICE_CANCEL or SIGNIN_CHOICE_NEW_PROFILE.
  // `process_user_choice_callback` is the callback that handles the user
  // choice. This callback may contain a callback to notify UI that that the
  // operation is done. If no UI notification is required, that callback does
  // not need to be set.
  // `done_callback` is the callback when the flow is complete, this is
  // where The UI cleanups should be handled.
  void ShowModalManagedUserNoticeDialog(
      std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
          create_param);

  // Shows the modal sign-in error dialog as a browser-modal dialog on top of
  // the |browser_|'s window.
  void ShowModalSigninErrorDialog();

  // Returns true if the modal dialog is shown.
  bool ShowsModalDialog();

  // Closes the tab-modal signin flow previously shown using this
  // SigninViewController, if one exists. Does nothing otherwise.
  void CloseModalSignin();

  // Sets the height of the modal signin dialog.
  void SetModalSigninHeight(int height);

  // Called by a `dialog_`' when it closes.
  void OnModalDialogClosed();

  base::WeakPtr<SigninViewController> AsWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(SignInViewControllerBrowserTest,
                           EmailConfirmationDefaultFocus);
  FRIEND_TEST_ALL_PREFIXES(SignInViewControllerBrowserTest,
                           ErrorDialogDefaultFocus);
  FRIEND_TEST_ALL_PREFIXES(SignInViewControllerBrowserTest,
                           EnterpriseConfirmationDefaultFocus);
  FRIEND_TEST_ALL_PREFIXES(SignInViewControllerBrowserOIDCAccountTest,
                           EnterpriseConfirmationDefaultFocus);
  FRIEND_TEST_ALL_PREFIXES(SignInViewControllerBrowserOIDCAccountTest,
                           EnterpriseConfirmationCancel);
  FRIEND_TEST_ALL_PREFIXES(SigninViewControllerDelegateViewsBrowserTest,
                           CloseImmediately);
  FRIEND_TEST_ALL_PREFIXES(ProfilePickerCreationFlowBrowserTest,
                           CreateLocalProfile);
  FRIEND_TEST_ALL_PREFIXES(ProfilePickerCreationFlowBrowserTest,
                           CancelLocalProfileCreation);
  friend class login_ui_test_utils::SigninViewControllerTestUtil;
  friend class SigninReauthViewControllerBrowserTest;
  friend class SigninInterceptFirstRunExperienceDialogBrowserTest;
  friend class SyncConfirmationUIDialogPixelTest;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Shows the DICE-specific sign-in flow: opens a Gaia sign-in webpage in a new
  // tab attached to |browser_|. |email_hint| may be empty.
  // If `redirect_url` is empty, the Google search URL is used as continue_url.
  // Internal URLs such as the NTP are only supported when `signin_reason` is
  // `signin_metrics::Reason::kSigninPrimaryAccount`.
  void ShowDiceSigninTab(signin_metrics::Reason signin_reason,
                         signin_metrics::AccessPoint access_point,
                         signin_metrics::PromoAction promo_action,
                         const std::string& email_hint,
                         const GURL& redirect_url);

  // Called by `SignoutOrReauthWithPrompt()` once the unsynced datatypes are
  // fetched.
  void SignoutOrReauthWithPromptWithUnsyncedDataTypes(
      signin_metrics::AccessPoint reauth_access_point,
      signin_metrics::ProfileSignout profile_signout_source,
      signin_metrics::SourceForRefreshTokenOperation token_signout_source,
      syncer::DataTypeSet unsynced_datatypes);

  void ShowChromeSigninDialogForExtensions(
      std::string_view extension_name,
      base::OnceClosure on_complete,
      const AccountInfo& account_info_for_promos,
      content::WebContents* contents);

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Returns the web contents of the modal dialog.
  content::WebContents* GetModalDialogWebContentsForTesting();

  // Returns the currently displayed modal dialog, or nullptr if no modal dialog
  // is currently displayed.
  SigninModalDialog* GetModalDialogForTesting();

  // Helper to create an on close callback for `SigninModalDialog`.
  base::OnceClosure GetOnModalDialogClosedCallback();

  // Browser owning this controller.
  raw_ptr<Browser> browser_;

  // Currently displayed modal dialog, or nullptr if none is displayed.
  std::unique_ptr<SigninModalDialog> dialog_;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::unique_ptr<NewTabWebContentsObserver> new_tab_web_contents_observer_;
#endif

  base::WeakPtrFactory<SigninViewController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_VIEW_CONTROLLER_H_
