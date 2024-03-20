// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_VIEW_IDS_H_
#define CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_VIEW_IDS_H_

namespace chromeos::mahi {

// IDs used for the views that compose the Mahi Menu UI.
// Use these for easy access to the views during the unittests.
// Note that these IDs are only guaranteed to be unique inside
// `MahiMenuView`.
enum ViewID {
  kSummaryButton = 1,
  kOutlineButton,
  kSettingsButton,
  kTextfield,
  kSubmitQuestionButton,
};

}  // namespace chromeos::mahi

#endif  // CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_VIEW_IDS_H_
