// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACCESSIBILITY_READ_ANYTHING_CONSTANTS_H_
#define CHROME_COMMON_ACCESSIBILITY_READ_ANYTHING_CONSTANTS_H_

#include <set>
#include <string>

#include "ui/accessibility/ax_mode.h"

// Various constants used throughout the Read Anything feature.
namespace string_constants {

extern const char kReadAnythingDefaultFontName[];
extern const char kLetterSpacingHistogramName[];
extern const char kLineSpacingHistogramName[];
extern const char kColorHistogramName[];
extern const char kFontNameHistogramName[];
extern const char kFontScaleHistogramName[];
extern const char kSettingsChangeHistogramName[];
extern const char kScrollEventHistogramName[];

extern const std::set<std::string> GetNonSelectableUrls();

}  // namespace string_constants

namespace {

// |ui::AXMode::kHTML| is needed for URL information.
// |ui::AXMode::kScreenReader| is needed for heading level information.
const ui::AXMode kReadAnythingAXMode =
    ui::AXMode::kWebContents | ui::AXMode::kHTML | ui::AXMode::kScreenReader;

// Group id for the toolbar
const int kToolbarGroupId = 0;

// Visual constants for Read Anything feature.
const int kInternalInsets = 8;
const int kSeparatorTopBottomPadding = 4;
const int kMinimumComboboxWidth = 110;

const int kButtonPadding = 2;
const int kIconSize = 16;
const int kFontSizeIconSize = kIconSize + kInternalInsets;
const int kColorsIconSize = 24;
const int kSpacingIconSize = 20;

// Font size in em
const double kReadAnythingDefaultFontScale = 1;
const double kReadAnythingMinimumFontScale = 0.5;
const double kReadAnythingMaximumFontScale = 4.5;
const double kReadAnythingFontScaleIncrement = 0.25;

// Enum for logging when a text style setting is changed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ReadAnythingSettingsChange {
  kFontChange = 0,
  kFontSizeChange = 1,
  kThemeChange = 2,
  kLineHeightChange = 3,
  kLetterSpacingChange = 4,
  kMaxValue = kLetterSpacingChange,
};

// Enum for logging how a scroll occurs.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ReadAnythingScrollEvent {
  kSelectedSidePanel = 0,
  kSelectedMainPanel = 1,
  kScrolledSidePanel = 2,
  kScrolledMainPanel = 3,
  kMaxValue = kScrolledMainPanel,
};

}  // namespace

#endif  // CHROME_COMMON_ACCESSIBILITY_READ_ANYTHING_CONSTANTS_H_
