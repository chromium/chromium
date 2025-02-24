// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_READ_ANYTHING_READ_ANYTHING_CONSTANTS_H_
#define CHROME_COMMON_READ_ANYTHING_READ_ANYTHING_CONSTANTS_H_

// Various constants used throughout the Read Anything feature.
namespace string_constants {

extern const char kReadAnythingPlaceholderFontName[];
extern const char kReadAnythingDefaultFont[];
extern const char kReadAnythingPlaceholderVoiceName[];
extern const char kLetterSpacingHistogramName[];
extern const char kLineSpacingHistogramName[];
extern const char kColorHistogramName[];
extern const char kFontNameHistogramName[];
extern const char kFontScaleHistogramName[];
extern const char kScrollEventHistogramName[];
extern const char kEmptyStateHistogramName[];
extern const char kLanguageHistogramName[];
extern const char kReadingModeName[];
}  // namespace string_constants

namespace read_anything {

// Audio constants for Read Aloud feature.
// Speech rate is a multiplicative scale where 1 is the baseline.
inline constexpr double kReadAnythingDefaultSpeechRate = 1;

// Display settings.
inline constexpr bool kReadAnythingDefaultLinksEnabled = true;
inline constexpr bool kReadAnythingDefaultImagesEnabled = false;

// Enum for logging how a scroll occurs.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ReadAnythingScrollEvent)
enum class ReadAnythingScrollEvent {
  kSelectedSidePanel = 0,
  kSelectedMainPanel = 1,
  kScrolledSidePanel = 2,
  kScrolledMainPanel = 3,
  kMaxValue = kScrolledMainPanel,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingScrollEvent)

// Enum for logging when we show the empty state.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ReadAnythingEmptyState)
enum class ReadAnythingEmptyState {
  kEmptyStateShown = 0,
  kSelectionAfterEmptyStateShown = 1,
  kMaxValue = kSelectionAfterEmptyStateShown,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingFontName)

}  // namespace read_anything

#endif  // CHROME_COMMON_READ_ANYTHING_READ_ANYTHING_CONSTANTS_H_
