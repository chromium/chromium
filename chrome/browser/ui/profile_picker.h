// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_PICKER_H_
#define CHROME_BROWSER_UI_PROFILE_PICKER_H_

class ProfilePicker {
 public:
  // Different pages to be displayed when the profile picker window opens.
  enum class Page {
    kManageProfiles,
    kAddNewProfile,
  };

  // Shows the Profile picker on the given `page` or re-activates an existing
  // one. In the latter case, the `page` parameter is ignored.
  static void Show(Page page = Page::kManageProfiles);

  // Hides the profile picker.
  static void Hide();

  // Returns whether the profile picker is currently open.
  static bool IsOpen();

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfilePicker);
};

#endif  // CHROME_BROWSER_UI_PROFILE_PICKER_H_
