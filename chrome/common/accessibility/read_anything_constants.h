// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACCESSIBILITY_READ_ANYTHING_CONSTANTS_H_
#define CHROME_COMMON_ACCESSIBILITY_READ_ANYTHING_CONSTANTS_H_

#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "ui/accessibility/ax_mode.h"

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

}  // namespace string_constants

// When adding a new font, add info to ReadAnythingFont, kReadAnythingFonts,
// and GetFontInfos() below.
namespace fonts {
// Enum for logging the user-chosen font.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ReadAnythingFont)
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
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingFontName)

// Holds compile-time known information about each of Read Anything's supported
// fonts. If num_langs_supported is 0, then that font supports all languages.
struct FontInfo {
  ReadAnythingFont logging_value;
  const char* css_name;
  const char* langs_supported[40];
  size_t num_langs_supported;
};

inline const char* kReadAnythingFonts[] = {
    "Poppins",       "Sans-serif",  "Serif",
    "Comic Neue",    "Lexend Deca", "EB Garamond",
    "STIX Two Text", "Andika",      "Atkinson Hyperlegible",
};
inline constexpr FontInfo kPoppinsFontInfo = {
    ReadAnythingFont::kPoppins,
    /*css_name=*/"Poppins",
    {"af", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
     "fr", "hi", "hr", "hu", "id", "it", "lt", "lv", "mr", "ms",
     "nl", "pl", "pt", "ro", "sk", "sl", "sv", "sw", "tr"},
    /*num_langs_supported=*/29};
inline constexpr FontInfo kSansSerifFontInfo = {ReadAnythingFont::kSansSerif,
                                                /*css_name=*/"sans-serif",
                                                {},
                                                /*num_langs_supported=*/0};
inline constexpr FontInfo kSerifFontInfo = {ReadAnythingFont::kSerif,
                                            /*css_name=*/"serif",
                                            {},
                                            /*num_langs_supported=*/0};
inline constexpr FontInfo kComicNeueFontInfo = {
    ReadAnythingFont::kComicNeue,
    /*css_name=*/"\"Comic Neue\"",
    {"af", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil", "fr", "hr",
     "hu", "id", "it", "ms", "nl", "pl", "pt", "sk", "sl", "sv",  "sw"},
    /*num_langs_supported=*/23};
inline constexpr FontInfo kLexendDecaFontInfo = {
    ReadAnythingFont::kLexendDeca,
    /*css_name=*/"\"Lexend Deca\"",
    {"af", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
     "fr", "hr", "hu", "id", "it", "lt", "lv", "ms", "nl", "pl",
     "pt", "ro", "sk", "sl", "sv", "sw", "tr", "vi"},
    /*num_langs_supported=*/28};
inline constexpr FontInfo kEbGaramondFontInfo = {
    ReadAnythingFont::kEbGaramond,
    /*css_name=*/"\"EB Garamond\"",
    {"af", "bg", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
     "fr", "hr", "hu", "id", "it", "lt", "lv", "ms", "nl", "pl", "pt",
     "ro", "ru", "sk", "sl", "sr", "sv", "sw", "tr", "uk", "vi"},
    /*num_langs_supported=*/32};
inline constexpr FontInfo kStixTwoTextFontInfo = {
    ReadAnythingFont::kStixTwoText,
    /*css_name=*/"STIX \"Two Text\"",
    {"af", "bg", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
     "fr", "hr", "hu", "id", "it", "lt", "lv", "ms", "nl", "pl", "pt",
     "ro", "ru", "sk", "sl", "sr", "sv", "sw", "tr", "uk", "vi"},
    /*num_langs_supported=*/32};
inline constexpr FontInfo kAndikaFontInfo = {
    ReadAnythingFont::kAndika,
    /*css_name=*/"Andika",
    {"af", "bg", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil", "fr",
     "hr", "hu", "id", "it", "kr", "lt", "lu", "lv", "ms", "nd", "nl",  "nr",
     "pl", "pt", "ro", "ru", "sk", "sl", "sr", "sv", "sw", "tr", "uk",  "vi"},
    /*num_langs_supported=*/36};
inline constexpr FontInfo kAtkinsonHyperlegibleFontInfo = {
    ReadAnythingFont::kAtkinsonHyperlegible,
    /*css_name=*/"Atkinson Hyperlegible",
    {"af", "ca", "cs", "da", "de", "en", "es", "et", "eu", "fi", "fil", "fr",
     "gl", "hr", "hu", "id", "is", "it", "kk", "lt", "ms", "nl", "no",  "pl",
     "pt", "pt", "ro", "sl", "sq", "sr", "sr", "sv", "sw", "zu"},
    /*num_langs_supported=*/34};
extern const base::fixed_flat_map<std::string_view, FontInfo, 9> kFontInfos;

}  // namespace fonts

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

}  // namespace

#endif  // CHROME_COMMON_ACCESSIBILITY_READ_ANYTHING_CONSTANTS_H_
