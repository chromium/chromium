// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_PUBLIC_SUPPORTED_LANGUAGES_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_PUBLIC_SUPPORTED_LANGUAGES_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"

namespace on_device_translation {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// The supported languages for on-device translation.
//
// LINT.IfChange(SupportedLanguage)
enum class SupportedLanguage {
  kEn = 0,
  kEs = 1,
  kJa = 2,
  kAr = 3,
  kBn = 4,
  kDe = 5,
  kFr = 6,
  kHi = 7,
  kIt = 8,
  kKo = 9,
  kNl = 10,
  kPl = 11,
  kPt = 12,
  kRu = 13,
  kTh = 14,
  kTr = 15,
  kVi = 16,
  kZh = 17,
  kZhHant = 18,
  kBg = 19,
  kCs = 20,
  kDa = 21,
  kEl = 22,
  kFi = 23,
  kHr = 24,
  kHu = 25,
  kId = 26,
  kIw = 27,
  kLt = 28,
  kNo = 29,
  kRo = 30,
  kSk = 31,
  kSl = 32,
  kSv = 33,
  kUk = 34,
  kKn = 35,
  kTa = 36,
  kTe = 37,
  kMr = 38,
  kMaxValue = kMr,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/translate/enums.xml:SupportedLanguage)

// The supported languages for on-device translation.
static constexpr auto kSupportedLanguageCodes =
    base::MakeFixedFlatSet<std::string_view>({
        "en", "es", "ja", "ar", "bn", "de", "fr", "hi", "it",      "ko",
        "nl", "pl", "pt", "ru", "th", "tr", "vi", "zh", "zh-Hant", "bg",
        "cs", "da", "el", "fi", "hr", "hu", "id", "iw", "lt",      "no",
        "ro", "sk", "sl", "sv", "uk", "kn", "ta", "te", "mr",
    });
static_assert(std::size(kSupportedLanguageCodes) ==
                  static_cast<unsigned>(SupportedLanguage::kMaxValue) + 1,
              "All languages must be in kSupportedLanguageCodes.");

// Converts a SupportedLanguage to a language code.
std::string_view ToLanguageCode(SupportedLanguage supported_language);

// Converts a language code to a SupportedLanguage.
std::optional<SupportedLanguage> ToSupportedLanguage(
    std::string_view language_code);

// Returns the language pair for a non-English supported language. The order of
// the language pair is determined by the language code's alphabetical order.
std::pair<SupportedLanguage, SupportedLanguage>
SupportedLanguagePairFromNonEnglishSupportedLanguage(
    SupportedLanguage supported_language);

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_PUBLIC_SUPPORTED_LANGUAGES_H_
