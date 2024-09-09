// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_DIALOG_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

class Profile;

namespace content {
class WebContents;
}

// A tab-modal dialog to ask the user to confirm their email before signing in.
class SigninEmailConfirmationDialog : public ui::WebDialogDelegate,
                                      public SigninViewControllerDelegate {
 public:
  // Actions that can be taken when the user is asked to confirm their account.
  enum Action {
    // The user chose not to sign in to the current profile and wants chrome
    // to create a new profile instead.
    CREATE_NEW_USER,

    // The user chose to sign in and enable sync in the current profile.
    START_SYNC,

    // The user chose abort sign in.
    CLOSE
  };

  // Callback indicating action performed by the user.
  using Callback = base::OnceCallback<void(Action)>;

  // Create and show the dialog, which owns itself.
  // Ask the user for confirmation before starting to sync.
  static SigninEmailConfirmationDialog* AskForConfirmation(
      content::WebContents* contents,
      Profile* profile,
      const std::string& last_email,
      const std::string& email,
      Callback callback);

  SigninEmailConfirmationDialog(const SigninEmailConfirmationDialog&) = delete;
  SigninEmailConfirmationDialog& operator=(
      const SigninEmailConfirmationDialog&) = delete;

  ~SigninEmailConfirmationDialog() override;

 private:
  class DialogWebContentsObserver;

  SigninEmailConfirmationDialog(content::WebContents* contents,
                                Profile* profile,
                                const std::string& last_email,
                                const std::string& new_email,
                                Callback callback);

  // WebDialogDelegate implementation.
  void OnDialogClosed(const std::string& json_retval) override;

  // SigninViewControllerDelegate:
  void CloseModalSignin() override;
  void ResizeNativeView(int height) override;
  content::WebContents* GetWebContents() override;
  void SetWebContents(content::WebContents* web_contents) override;

  // Shows the dialog and releases ownership of this object. Another object will
  // take ownership and delete this object.
  void ShowDialog();

  // Closes the dialog.
  void CloseDialog();

  // Resets the dialog observer.
  void ResetDialogObserver();

  // Returns the media router dialog WebContents.
  // Returns nullptr if there is no dialog.
  content::WebContents* GetDialogWebContents() const;

  // Web contents from which the "Learn more" link should be opened.
  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<Profile> profile_;

  Callback callback_;

  // Observer for lifecycle events of the web contents of the dialog.
  std::unique_ptr<DialogWebContentsObserver> dialog_observer_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_EMAIL_CONFIRMATION_DIALOG_H_
