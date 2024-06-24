// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACCESSIBILITY_READ_ANYTHING_CONSTANTS_H_
#define CHROME_COMMON_ACCESSIBILITY_READ_ANYTHING_CONSTANTS_H_

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
extern const char kScrollEventHistogramName[];
extern const char kEmptyStateHistogramName[];
extern const char kLanguageHistogramName[];

}  // namespace string_constants

namespace {

// Used for text formatting correction in PDFs. This value should match the line
// width limit in app.html.
inline constexpr int kMaxLineWidth = 60;

// Audio constants for Read Aloud feature.
// Speech rate is a multiplicative scale where 1 is the baseline.
inline constexpr double kReadAnythingDefaultSpeechRate = 1;

// Font size in em
inline constexpr double kReadAnythingDefaultFontScale = 1;
inline constexpr double kReadAnythingMinimumFontScale = 0.5;
inline constexpr double kReadAnythingMaximumFontScale = 4.5;
inline constexpr double kReadAnythingFontScaleIncrement = 0.25;

// Display settings.
inline constexpr bool kReadAnythingDefaultLinksEnabled = true;
inline constexpr bool kReadAnythingDefaultImagesEnabled = false;

// List of fonts supported by Read Anything in the order they should be
// displayed.
// TODO(crbug.com/348497904): Consolidate with duplicated lists in common.ts and
// read_anything_untrusted_page_handler.h.
const char* kReadAnythingFonts[] = {
    "Poppins",       "Sans-serif",  "Serif",
    "Comic Neue",    "Lexend Deca", "EB Garamond",
    "STIX Two Text", "Andika",      "Atkinson Hyperlegible",
};

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

const char* kLanguagesSupportedByAndika[] = {
    "af", "bg", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil", "fr",
    "hr", "hu", "id", "it", "kr", "lt", "lu", "lv", "ms", "nd", "nl",  "nr",
    "pl", "pt", "ro", "ru", "sk", "sl", "sr", "sv", "sw", "tr", "uk",  "vi"};

const char* kLanguagesSupportedByAtkinsonHyperlegible[] = {
    "af", "ca", "cs", "da", "de", "en", "es", "et", "eu", "fi", "fil", "fr",
    "gl", "hr", "hu", "id", "is", "it", "kk", "lt", "ms", "nl", "no",  "pl",
    "pt", "pt", "ro", "sl", "sq", "sr", "sr", "sv", "sw", "zu"};

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
  kAndika = 7,
  kAtkinsonHyperlegible = 8,
  kMaxValue = kAtkinsonHyperlegible,
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
