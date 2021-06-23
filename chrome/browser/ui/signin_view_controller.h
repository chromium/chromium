// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#endif

#if defined(OS_ANDROID)
#error This file should only be included on desktop.
#endif

class Browser;
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

// Class responsible for showing and hiding all sign-in related UIs
// (modal sign-in, DICE full-tab sign-in page, sync confirmation dialog, sign-in
// error dialog, reauth prompt). Sync confirmation is used on
// Win/Mac/Linux/Chrome OS. Sign-in is only used on Win/Mac/Linux because
// Chrome OS has its own sign-in flow and doesn't use DICE.
class SigninViewController : public SigninViewControllerDelegate::Observer {
 public:
  // Handle that will stop ongoing reauths upon destruction.
  class ReauthAbortHandle {
   public:
    virtual ~ReauthAbortHandle() = default;
  };

  explicit SigninViewController(Browser* browser);
  ~SigninViewController() override;

  // Returns true if the signin flow should be shown for |mode|.
  static bool ShouldShowSigninForMode(profiles::BubbleViewMode mode);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Shows the signin attached to |browser_|'s active web contents.
  // |access_point| indicates the access point used to open the Gaia sign in
  // page.
  // DEPRECATED: Use ShowDiceEnableSyncTab instead.
  void ShowSignin(profiles::BubbleViewMode mode,
                  signin_metrics::AccessPoint access_point,
                  const GURL& redirect_url = GURL::EmptyGURL());

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

  // Shows the modal sign-in email confirmation dialog as a tab-modal dialog on
  // top of the currently displayed WebContents in |browser_|.
  void ShowModalSigninEmailConfirmationDialog(
      const std::string& last_email,
      const std::string& email,
      base::OnceCallback<void(SigninEmailConfirmationDialog::Action)> callback);

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
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Shows the modal sync confirmation dialog as a browser-modal dialog on top
  // of the |browser_|'s window.
  void ShowModalSyncConfirmationDialog();

  // Shows the modal enterprise confirmation dialog as a browser-modal dialog on
  // top of the `browser_`'s window. `domain_name` is the domain of the
  // enterprise account being shown. `callback` is called with the user's action
  // on the dialog.
  void ShowModalEnterpriseConfirmationDialog(
      const std::string& domain_name,
      SkColor profile_color,
      base::OnceCallback<void(bool)> callback);

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

  // SigninViewControllerDelegate::Observer:
  void OnModalSigninClosed() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SignInViewControllerBrowserTest,
                           ErrorDialogDefaultFocus);
  FRIEND_TEST_ALL_PREFIXES(SignInViewControllerBrowserTest,
                           EnterpriseConfirmationDefaultFocus);
  friend class login_ui_test_utils::SigninViewControllerTestUtil;
  friend class SigninReauthViewControllerBrowserTest;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Shows the DICE-specific sign-in flow: opens a Gaia sign-in webpage in a new
  // tab attached to |browser_|. |email_hint| may be empty.
  void ShowDiceSigninTab(signin_metrics::Reason signin_reason,
                         signin_metrics::AccessPoint access_point,
                         signin_metrics::PromoAction promo_action,
                         const std::string& email_hint,
                         const GURL& redirect_url = GURL::EmptyGURL());
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Returns the web contents of the modal dialog.
  content::WebContents* GetModalDialogWebContentsForTesting();

  // Returns the modal dialog delegate.
  SigninViewControllerDelegate* GetModalDialogDelegateForTesting();

  // Browser owning this controller.
  Browser* browser_;

  // |delegate_| owns itself and calls OnModalSigninClosed() before being
  // destroyed.
  SigninViewControllerDelegate* delegate_ = nullptr;
  base::ScopedObservation<SigninViewControllerDelegate,
                          SigninViewControllerDelegate::Observer>
      delegate_observation_{this};

  base::WeakPtrFactory<SigninViewController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SigninViewController);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_H_
