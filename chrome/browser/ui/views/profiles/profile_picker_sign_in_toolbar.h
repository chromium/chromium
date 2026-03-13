// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGN_IN_TOOLBAR_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGN_IN_TOOLBAR_H_

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kProfilePickerSignInToolbarDontSignInButtonElementId);

// Class responsible for the top toolbar shown during the GAIA sign-in within
// profile creation flow.
class ProfilePickerSignInToolbar : public views::View {
  METADATA_HEADER(ProfilePickerSignInToolbar, views::View)

 public:
  ProfilePickerSignInToolbar();
  ~ProfilePickerSignInToolbar() override;
  ProfilePickerSignInToolbar(const ProfilePickerSignInToolbar&) = delete;
  ProfilePickerSignInToolbar& operator=(const ProfilePickerSignInToolbar&) =
      delete;

  // Builds the actual toolbar, before calling this function, it remains empty.
  // `on_back_callback` is called when the user clicks on the back button
  // (MUST be set to a non-null callback).
  // `on_dont_sign_in_callback` is called when the user clicks on
  // the "Don't sign in" button, if null, the button is not created.
  void BuildToolbar(base::RepeatingClosure on_back_callback,
                    base::RepeatingClosure on_dont_sign_in_callback);

  // Changes the visibility of the "Don't sign in" button. It's no-op if the
  // button is not created (see `BuildToolbar` method).
  void SetDontSignInButtonVisible(bool visible);

 private:
  raw_ptr<views::View> dont_sign_in_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGN_IN_TOOLBAR_H_
