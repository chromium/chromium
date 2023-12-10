// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FORCE_SIGNIN_DIALOG_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FORCE_SIGNIN_DIALOG_HOST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/profiles/profile_picker_force_signin_dialog_delegate.h"
#include "ui/gfx/native_widget_types.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class DialogDelegateView;
}

class Profile;

// Class encapsulating logic for views willing to host
// ProfilePickerForceSigninDialogDelegate.
class ProfilePickerForceSigninDialogHost {
 public:
  ProfilePickerForceSigninDialogHost();

  ProfilePickerForceSigninDialogHost(
      const ProfilePickerForceSigninDialogHost&) = delete;
  ProfilePickerForceSigninDialogHost& operator=(
      const ProfilePickerForceSigninDialogHost&) = delete;

  // Shows a dialog where the user can auth the profile or see the auth error
  // message. If a dialog is already shown, this destroys the current dialog and
  // creates a new one.
  void ShowDialog(Profile* profile, const GURL& url, gfx::NativeView parent);

  // Hides the reauth dialog if it is showing.
  void HideDialog();

  // Displays a sign in error message that is created by Chrome but not GAIA
  // without browser window. If the dialog is not currently shown, this does
  // nothing.
  void DisplayErrorMessage();

  views::DialogDelegateView* GetDialogDelegateViewForTesting() const;
  content::WebContents* get_web_contents_for_testing() const {
    return delegate_->GetWebContentsForTesting();
  }

 private:
  friend class ProfilePickerForceSigninDialogDelegate;

  // Resets delegate_ to nullptr when delegate_ is no longer alive.
  void OnDialogDestroyed();

  // Owned by the view hierarchy.
  raw_ptr<ProfilePickerForceSigninDialogDelegate> delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FORCE_SIGNIN_DIALOG_HOST_H_
