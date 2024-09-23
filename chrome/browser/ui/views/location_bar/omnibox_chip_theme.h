// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_THEME_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_THEME_H_

// Icon, text, and background colors that should be used for different types
// of Chip.
enum class OmniboxChipTheme {
  kLowVisibility,
  // Shows the chip with no background, and an icon color matching other icons
  // in the omnibox. Suitable for collapsing the chip down to a less prominent
  // icon.
  kIconStyle,
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_CHIP_THEME_H_
