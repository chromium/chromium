// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_CHIP_THEME_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_CHIP_THEME_H_

// Icon, text, and background colors that should be used for different types
// of PermissionChipView.
enum class PermissionChipTheme {
  // Used for the permission requests / confirmation chip.
  kNormalVisibility,
  // Used for the quiet chip and for blocked activity indicators.
  kLowVisibility,
  // Used for in-use activity indicators.
  kInUseActivityIndicator,
  // Used for blocked activity indicators.
  kBlockedActivityIndicator,
  // Used for system-level blocked activity indicators.
  kOnSystemBlockedActivityIndicator,
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_CHIP_THEME_H_
