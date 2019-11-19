// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "url/gurl.h"

class Browser;
class SigninViewControllerDelegate;

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
}  // namespace signin_metrics

// Class responsible for showing and hiding all sign-in related UIs
// (modal sign-in, DICE full-tab sign-in page, sync confirmation dialog, sign-in
// error dialog).
// This is only used on desktop platforms, not used on Android and ChromeOS.
class SigninViewController {
 public:
  SigninViewController();
  virtual ~SigninViewController();

  // Returns true if the signin flow should be shown for |mode|.
  static bool ShouldShowSigninForMode(profiles::BubbleViewMode mode);

  // Shows the signin attached to |browser|'s active web contents.
  // |access_point| indicates the access point used to open the Gaia sign in
  // page.
  // DEPRECATED: Use ShowDiceEnableSyncTab instead.
  void ShowSignin(profiles::BubbleViewMode mode,
                  Browser* browser,
                  signin_metrics::AccessPoint access_point,
                  const GURL& redirect_url = GURL::EmptyGURL());

  // Shows a Chrome Sync signin tab. |email_hint| may be empty.
  // Note: If the user has already set a primary account, then this is
  // considered a reauth of the primary account, and |email_hint| is ignored.
  void ShowDiceEnableSyncTab(Browser* browser,
                             signin_metrics::AccessPoint access_point,
                             signin_metrics::PromoAction promo_action,
                             const std::string& email_hint);

  // Shows the Dice "add account" tab, which adds an account to the browser but
  // does not turn sync on. |email_hint| may be empty.
  void ShowDiceAddAccountTab(Browser* browser,
                             signin_metrics::AccessPoint access_point,
                             const std::string& email_hint);

  // Shows the modal sync confirmation dialog as a browser-modal dialog on top
  // of the |browser|'s window.
  void ShowModalSyncConfirmationDialog(Browser* browser);

  // Shows the modal sign-in error dialog as a browser-modal dialog on top of
  // the |browser|'s window.
  void ShowModalSigninErrorDialog(Browser* browser);

  // Returns true if the modal dialog is shown.
  bool ShowsModalDialog();

  // Closes the tab-modal signin flow previously shown using this
  // SigninViewController, if one exists. Does nothing otherwise.
  void CloseModalSignin();

  // Sets the height of the modal signin dialog.
  void SetModalSigninHeight(int height);

  // Notifies this object that it's |delegate_| member has become invalid.
  void ResetModalSigninDelegate();

 private:
  // Shows the DICE-specific sign-in flow: opens a Gaia sign-in webpage in a new
  // tab attached to |browser|. |email_hint| may be empty.
  void ShowDiceSigninTab(Browser* browser,
                         signin_metrics::Reason signin_reason,
                         signin_metrics::AccessPoint access_point,
                         signin_metrics::PromoAction promo_action,
                         const std::string& email_hint,
                         const GURL& redirect_url = GURL::EmptyGURL());

  friend class login_ui_test_utils::SigninViewControllerTestUtil;

  // Returns the web contents of the modal dialog.
  content::WebContents* GetModalDialogWebContentsForTesting();

  SigninViewControllerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SigninViewController);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_H_
