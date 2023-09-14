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

extern const char kReadAnythingPlaceholderFontName[];
extern const char kReadAnythingPlaceholderVoiceName[];
extern const char kLetterSpacingHistogramName[];
extern const char kLineSpacingHistogramName[];
extern const char kColorHistogramName[];
extern const char kFontNameHistogramName[];
extern const char kFontScaleHistogramName[];
extern const char kSettingsChangeHistogramName[];
extern const char kScrollEventHistogramName[];
extern const char kEmptyStateHistogramName[];
extern const char kLanguageHistogramName[];

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

// Audio constants for Read Aloud feature.
// Speech rate is a multiplicative scale where 1 is the baseline.
const double kReadAnythingDefaultSpeechRate = 1;

// Font size in em
const double kReadAnythingDefaultFontScale = 1;
const double kReadAnythingMinimumFontScale = 0.5;
const double kReadAnythingMaximumFontScale = 4.5;
const double kReadAnythingFontScaleIncrement = 0.25;

const char* kLanguagesSupportedByPoppins[] = {
    "af", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
    "fr", "hi", "hr", "hu", "id", "it", "lt", "lv", "mr", "ms",
    "nl", "pl", "pt", "ro", "sk", "sl", "sv", "sw", "tr"};

const char* kLanguagesSupportedByComicNeue[] = {
    "af", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil", "fr", "hr",
    "hu", "id", "it", "ms", "nl", "pl", "pt", "sk", "sl", "sv", "sw"};

const char* kLanguagesSupportedByLexendDeca[] = {
    "af", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
    "fr", "hr", "hu", "id", "it", "lt", "lv", "ms", "nl", "pl",
    "pt", "ro", "sk", "sl", "sv", "sw", "tr", "vi"};

const char* kLanguagesSupportedByEbGaramond[] = {
    "af", "bg", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
    "fr", "hr", "hu", "id", "it", "lt", "lv", "ms", "nl", "pl", "pt",
    "ro", "ru", "sk", "sl", "sr", "sv", "sw", "tr", "uk", "vi"};

const char* kLanguagesSupportedByStixTwoText[] = {
    "af", "bg", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
    "fr", "hr", "hu", "id", "it", "lt", "lv", "ms", "nl", "pl", "pt",
    "ro", "ru", "sk", "sl", "sr", "sv", "sw", "tr", "uk", "vi"};

// Enum for logging when a text style setting is changed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/1465029): Remove this enum once the views toolbar is removed.
enum class ReadAnythingSettingsChange {
  kFontChange = 0,
  kFontSizeChange = 1,
  kThemeChange = 2,
  kLineHeightChange = 3,
  kLetterSpacingChange = 4,
  kMaxValue = kLetterSpacingChange,
};

// Enum for logging the user-chosen font.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ReadAnythingFont {
  kPoppins = 0,
  kSansSerif = 1,
  kSerif = 2,
  kComicNeue = 3,
  kLexendDeca = 4,
  kEbGaramond = 5,
  kStixTwoText = 6,
  kMaxValue = kStixTwoText,
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

// Enum for logging when we show the empty state.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ReadAnythingEmptyState {
  kEmptyStateShown = 0,
  kSelectionAfterEmptyStateShown = 1,
  kMaxValue = kSelectionAfterEmptyStateShown,
};

}  // namespace

#endif  // CHROME_COMMON_ACCESSIBILITY_READ_ANYTHING_CONSTANTS_H_
