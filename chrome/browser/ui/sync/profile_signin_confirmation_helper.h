// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_PROFILE_SIGNIN_CONFIRMATION_HELPER_H_
#define CHROME_BROWSER_UI_SYNC_PROFILE_SIGNIN_CONFIRMATION_HELPER_H_

#include "base/callback.h"
#include "third_party/skia/include/core/SkColor.h"

class Profile;

namespace ui {

class NativeTheme;

// Create slightly different colors for the dialog prompt bar.
SkColor GetSigninConfirmationPromptBarColor(NativeTheme* theme, SkAlpha alpha);

// Determines whether the browser has ever been shutdown since the
// profile was created.
// Exposed for testing.
bool HasBeenShutdown(Profile* profile);

// Determines whether there are any synced extensions installed (that
// shouldn't be ignored).
// Exposed for testing.
bool HasSyncedExtensions(Profile* profile);

// Determines whether the user should be prompted to create a new
// profile before signin.
void CheckShouldPromptForNewProfile(Profile* profile,
                                    base::OnceCallback<void(bool)> cb);

// Handles user input from confirmation dialog.
class ProfileSigninConfirmationDelegate {
 public:
  virtual ~ProfileSigninConfirmationDelegate();
  virtual void OnCancelSignin() = 0;
  virtual void OnContinueSignin() = 0;
  virtual void OnSigninWithNewProfile() = 0;
};

}  // namespace ui

#endif  // CHROME_BROWSER_UI_SYNC_PROFILE_SIGNIN_CONFIRMATION_HELPER_H_
