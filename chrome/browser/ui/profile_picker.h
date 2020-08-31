// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_PICKER_H_
#define CHROME_BROWSER_UI_PROFILE_PICKER_H_

class ProfilePicker {
 public:
  // An entry point that triggers the profile picker window to open.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class EntryPoint {
    kOnStartup = 0,
    kProfileMenuManageProfiles = 1,
    kProfileMenuAddNewProfile = 2,
    kOpenNewWindowAfterProfileDeletion = 3,
    kMaxValue = kOpenNewWindowAfterProfileDeletion,
  };

  // Shows the Profile picker for the given `entry_point` or re-activates an
  // existing one. In the latter case, the displayed page is not updated.
  static void Show(EntryPoint entry_point);

  // Hides the profile picker.
  static void Hide();

  // Returns whether the profile picker is currently open.
  static bool IsOpen();

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfilePicker);
};

#endif  // CHROME_BROWSER_UI_PROFILE_PICKER_H_
