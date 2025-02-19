// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/read_anything/read_anything_util.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"

namespace {

// All values are in em.
constexpr double kMinScale = 0.5;
constexpr double kMaxScale = 4.5;
constexpr double kScaleStepSize = 0.25;

int AsSteps(double font_scale) {
  return base::ClampRound((font_scale - kMinScale) / kScaleStepSize);
}

}  // namespace

std::vector<std::string> GetSupportedFonts(std::string_view language_code) {
  // If you modify the set of fonts here, also change `LogFontName()` below.
  static constexpr auto kPoppinsLangs =
      base::MakeFixedFlatSet<std::string_view>(
          {"af", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
           "fr", "hi", "hr", "hu", "id", "it", "lt", "lv", "mr", "ms",
           "nl", "pl", "pt", "ro", "sk", "sl", "sv", "sw", "tr"});
  static constexpr auto kComicNeueLangs =
      base::MakeFixedFlatSet<std::string_view>(
          {"af", "ca",  "cs", "da", "de", "en", "es", "et",
           "fi", "fil", "fr", "hr", "hu", "id", "it", "ms",
           "nl", "pl",  "pt", "sk", "sl", "sv", "sw"});
  static constexpr auto kLexendDecaLangs =
      base::MakeFixedFlatSet<std::string_view>(
          {"af", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
           "fr", "hr", "hu", "id", "it", "lt", "lv", "ms", "nl", "pl",
           "pt", "ro", "sk", "sl", "sv", "sw", "tr", "vi"});
  static constexpr auto kEbGaramondLangs =
      base::MakeFixedFlatSet<std::string_view>(
          {"af", "bg", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
           "fr", "hr", "hu", "id", "it", "lt", "lv", "ms", "nl", "pl", "pt",
           "ro", "ru", "sk", "sl", "sr", "sv", "sw", "tr", "uk", "vi"});
  static constexpr auto kStixTwoTextLangs =
      base::MakeFixedFlatSet<std::string_view>(
          {"af", "bg", "ca", "cs", "da", "de", "en", "es", "et", "fi", "fil",
           "fr", "hr", "hu", "id", "it", "lt", "lv", "ms", "nl", "pl", "pt",
           "ro", "ru", "sk", "sl", "sr", "sv", "sw", "tr", "uk", "vi"});
  static constexpr auto kAndikaLangs = base::MakeFixedFlatSet<std::string_view>(
      {"af", "bg",  "ca", "cs", "da", "de", "en", "es", "et",
       "fi", "fil", "fr", "hr", "hu", "id", "it", "kr", "lt",
       "lu", "lv",  "ms", "nd", "nl", "nr", "pl", "pt", "ro",
       "ru", "sk",  "sl", "sr", "sv", "sw", "tr", "uk", "vi"});
  static constexpr auto kAtkinsonHyperlegibleLangs =
      base::MakeFixedFlatSet<std::string_view>(
          {"af", "ca", "cs", "da", "de", "en", "es", "et", "eu", "fi", "fil",
           "fr", "gl", "hr", "hu", "id", "is", "it", "kk", "lt", "ms", "nl",
           "no", "pl", "pt", "ro", "sl", "sq", "sr", "sv", "sw", "zu"});

  std::vector<std::string> font_choices = {"Sans-serif", "Serif"};
  if (language_code.empty() || kPoppinsLangs.contains(language_code)) {
    // Make this default by putting it first.
    font_choices.insert(font_choices.begin(), "Poppins");
  }
  if (language_code.empty() || kComicNeueLangs.contains(language_code)) {
    font_choices.push_back("Comic Neue");
  }
  if (language_code.empty() || kLexendDecaLangs.contains(language_code)) {
    font_choices.push_back("Lexend Deca");
  }
  if (language_code.empty() || kEbGaramondLangs.contains(language_code)) {
    font_choices.push_back("EB Garamond");
  }
  if (language_code.empty() || kStixTwoTextLangs.contains(language_code)) {
    font_choices.push_back("STIX Two Text");
  }
  if (language_code.empty() || kAndikaLangs.contains(language_code)) {
    font_choices.push_back("Andika");
  }
  if (language_code.empty() ||
      kAtkinsonHyperlegibleLangs.contains(language_code)) {
    font_choices.push_back("Atkinson Hyperlegible");
  }

  return font_choices;
}

void LogFontName(std::string_view font_name) {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // If you modify the set of fonts here, also change `GetSupportedFonts()`
  // above.
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
  // LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:ReadAnythingFontName)
  static constexpr auto kFontMap =
      base::MakeFixedFlatMap<std::string_view, ReadAnythingFont>(
          {{"Poppins", ReadAnythingFont::kPoppins},
           {"Sans-serif", ReadAnythingFont::kSansSerif},
           {"Serif", ReadAnythingFont::kSerif},
           {"Comic Neue", ReadAnythingFont::kComicNeue},
           {"Lexend Deca", ReadAnythingFont::kLexendDeca},
           {"EB Garamond", ReadAnythingFont::kEbGaramond},
           {"STIX Two Text", ReadAnythingFont::kStixTwoText},
           {"Andika", ReadAnythingFont::kAndika},
           {"Atkinson Hyperlegible", ReadAnythingFont::kAtkinsonHyperlegible}});
  if (const auto it = kFontMap.find(font_name); it != kFontMap.end()) {
    base::UmaHistogramEnumeration("Accessibility.ReadAnything.FontName",
                                  it->second);
  }
}

double AdjustFontScale(double font_scale, int increment) {
  return std::clamp(
      kMinScale + (AsSteps(font_scale) + increment) * kScaleStepSize, kMinScale,
      kMaxScale);
}

void LogFontScale(double font_scale) {
  base::UmaHistogramExactLinear("Accessibility.ReadAnything.FontScale",
                                AsSteps(font_scale), AsSteps(kMaxScale) + 1);
}
