// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/profiles/user_manager_profile_dialog_host.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"

class ScopedKeepAlive;
class UserManagerView;
class UserManagerProfileDialogDelegate;

namespace base {
class FilePath;
}

namespace views {
class WebView;
}

// Dialog widget that contains the Desktop User Manager webui.
class UserManagerView : public views::DialogDelegateView {
 public:
  // Do not call directly. To display the User Manager, use UserManager::Show().
  UserManagerView();

  // Creates a new UserManagerView instance for the |system_profile| and shows
  // the |url|.
  static void OnSystemProfileCreated(std::unique_ptr<UserManagerView> instance,
                                     base::AutoReset<bool>* pending,
                                     Profile* system_profile,
                                     const std::string& url);

  void set_user_manager_started_showing(
      const base::Time& user_manager_started_showing) {
    user_manager_started_showing_ = user_manager_started_showing;
  }

  // Logs how long it took the UserManager to open.
  void LogTimeToOpen();

  // Hides the reauth dialog if it is showing.
  void HideDialog();

  // Shows a dialog where the user can auth the profile or see the auth error
  // message. If a dialog is already shown, this destroys the current dialog and
  // creates a new one.
  void ShowDialog(content::BrowserContext* browser_context,
                  const GURL& url,
                  const base::FilePath& profile_path);

  // Displays sign in error message that is created by Chrome but not GAIA
  // without browser window. If the dialog is not currently shown, this does
  // nothing.
  void DisplayErrorMessage();

  // Getter of the path of profile which is selected in user manager for first
  // time signin.
  base::FilePath GetSigninProfilePath();

 private:
  friend class UserManagerProfileDialogDelegate;
  friend std::default_delete<UserManagerView>;

  ~UserManagerView() override;

  // Creates dialog and initializes UI.
  void Init(Profile* guest_profile, const GURL& url);

  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size CalculatePreferredSize() const override;

  // views::DialogDelegateView:
  void WindowClosing() override;

  views::WebView* web_view_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  base::Time user_manager_started_showing_;

  UserManagerProfileDialogHost dialog_host_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_
