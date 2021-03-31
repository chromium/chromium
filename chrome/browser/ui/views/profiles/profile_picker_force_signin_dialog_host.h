// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FORCE_SIGNIN_DIALOG_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FORCE_SIGNIN_DIALOG_HOST_H_

#include "base/files/file_path.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class ProfilePickerForceSigninDialogDelegate;

namespace content {
class BrowserContext;
}

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
  void ShowDialog(content::BrowserContext* browser_context,
                  const GURL& url,
                  const base::FilePath& profile_path,
                  gfx::NativeView parent);

  // Hides the reauth dialog if it is showing.
  void HideDialog();

  // Displays a sign in error message that is created by Chrome but not GAIA
  // without browser window. If the dialog is not currently shown, this does
  // nothing.
  void DisplayErrorMessage();

  // Getter of the path of profile which is selected in profile picker for force
  // signin.
  base::FilePath GetForceSigninProfilePath() const;

 private:
  friend class ProfilePickerForceSigninDialogDelegate;

  // Resets delegate_ to nullptr when delegate_ is no longer alive.
  void OnDialogDestroyed();

  // Owned by the view hierarchy.
  ProfilePickerForceSigninDialogDelegate* delegate_ = nullptr;

  // The path of profile that is being force signed in.
  base::FilePath force_signin_profile_path_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_FORCE_SIGNIN_DIALOG_HOST_H_
