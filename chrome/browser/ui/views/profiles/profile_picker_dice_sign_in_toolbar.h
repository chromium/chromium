// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_SIGN_IN_TOOLBAR_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_SIGN_IN_TOOLBAR_H_

#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// Class responsible for the top toolbar shown during the GAIA sign-in within
// profile creation flow.
class ProfilePickerDiceSignInToolbar : public views::View {
  METADATA_HEADER(ProfilePickerDiceSignInToolbar, views::View)

 public:
  ProfilePickerDiceSignInToolbar();
  ~ProfilePickerDiceSignInToolbar() override;
  ProfilePickerDiceSignInToolbar(const ProfilePickerDiceSignInToolbar&) =
      delete;
  ProfilePickerDiceSignInToolbar& operator=(
      const ProfilePickerDiceSignInToolbar&) = delete;

  // Builds the actual toolbar, before calling this function, it remains empty.
  void BuildToolbar(base::RepeatingClosure on_back_callback);

  // views::View:
  void OnThemeChanged() override;

 private:
  // Updates the colors for the toolbar.
  void UpdateToolbarColor();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_SIGN_IN_TOOLBAR_H_
