// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_CONSTANTS_H_

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

// Metrics
// Contains the types of button existed in Mahi Menu View. Note: this should
// be kept in sync with `MahiMenuButton` enum in
// tools/metrics/histograms/metadata/chromeos/enums.xml
enum class MahiMenuButton {
  kSummaryButton = 0,
  kOutlineButton = 1,
  kSubmitQuestionButton = 2,
  kCondensedMenuButton = 3,
  kSettingsButton = 4,
  kMaxValue = kSettingsButton,
};

inline constexpr char kMahiContextMenuButtonClickHistogram[] =
    "ChromeOS.Mahi.ContextMenuView.ButtonClicked";
inline constexpr char kMahiContextMenuDistillableHistogram[] =
    "ChromeOS.Mahi.ContextMenuView.Distillable";

}  // namespace chromeos::mahi

#endif  // CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_CONSTANTS_H_
