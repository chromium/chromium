// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_MANAGER_H_
#define CHROME_BROWSER_UI_USER_MANAGER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_window.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents_delegate.h"

namespace base {
class FilePath;
}

// Cross-platform methods for displaying the user manager.
class UserManager {
 public:
  // TODO(noms): Figure out if this size can be computed dynamically or adjusted
  // for smaller screens.
  static constexpr int kWindowWidth = 800;
  static constexpr int kWindowHeight = 600;

  // Shows the User Manager or re-activates an existing one, focusing the
  // profile given by |profile_path_to_focus|; passing an empty base::FilePath
  // focuses no user pod. Depending on the value of |user_manager_action|,
  // executes an action once the user manager displays or after a profile is
  // opened.
  static void Show(const base::FilePath& profile_path_to_focus,
                   profiles::UserManagerAction user_manager_action);

  // Hides the User Manager.
  static void Hide();

  // Returns whether the User Manager is showing and active.
  // TODO(zmin): Rename the function to something less confusing.
  // https://crbug.com/649380.
  static bool IsShowing();

  // To be called once the User Manager's contents are showing.
  static void OnUserManagerShown();

  // Add a callback that will be called the next time OnUserManagerShown is
  // called.
  static void AddOnUserManagerShownCallbackForTesting(
      base::OnceClosure callback);

  // Get the path of profile that is being signed in.
  static base::FilePath GetSigninProfilePath();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UserManager);
};

// Dialog that will be displayed when a profile is selected in UserManager.
class UserManagerProfileDialog {
 public:
  // Dimensions of the reauth dialog displaying the password-separated signin
  // flow.
  static constexpr int kDialogHeight = 512;
  static constexpr int kDialogWidth = 448;

  // Shows a dialog where the user can re-authenticate the profile with the
  // given |email|. This is called from the user manager when a profile is
  // locked and the user's password is detected to have been changed.
  static void ShowUnlockDialog(content::BrowserContext* browser_context,
                               const std::string& email);

  // Shows a reauth dialog with profile path so that the sign in error message
  // can be displayed without browser window.
  static void ShowUnlockDialogWithProfilePath(
      content::BrowserContext* browser_context,
      const std::string& email,
      const base::FilePath& profile_path);

  // Shows a dialog where the user logs into their profile for the first time
  // via the user manager, when force signin is enabled.
  static void ShowForceSigninDialog(content::BrowserContext* browser_context,
                                    const base::FilePath& profile_path);

  // Show the dialog and display local sign in error message without browser.
  static void ShowDialogAndDisplayErrorMessage(
      content::BrowserContext* browser_context);

  // Display local sign in error message without browser.
  static void DisplayErrorMessage();

  // Hides the dialog if it is showing.
  static void HideDialog();

  // Abstract base class for performing online reauthentication of profiles in
  // the User Manager. It is concretely implemented in UserManagerMac and
  // UserManagerView to specialize the closing of the UI's dialog widgets.
  class BaseDialogDelegate : public content::WebContentsDelegate {
   public:
    BaseDialogDelegate();

    // content::WebContentsDelegate:
    bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                           const content::ContextMenuParams& params) override;

    // content::WebContentsDelegate:
    void LoadingStateChanged(content::WebContents* source,
                             bool to_different_document) override;

   protected:
    virtual void CloseDialog() = 0;

    // WebContents of the embedded WebView.
    content::WebContents* guest_web_contents_;

    DISALLOW_COPY_AND_ASSIGN(BaseDialogDelegate);
  };
};

#endif  // CHROME_BROWSER_UI_USER_MANAGER_H_
