// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/public/supported_languages.h"

#include <utility>

#include "base/check_op.h"

namespace on_device_translation {
namespace {

// Map from `SupportedLanguage` to the language code.
inline constexpr auto kSupportedLanguageCodeMap = base::MakeFixedFlatMap<
    SupportedLanguage,
    std::string_view>(
    {{SupportedLanguage::kEn, "en"},          {SupportedLanguage::kEs, "es"},
     {SupportedLanguage::kJa, "ja"},          {SupportedLanguage::kAr, "ar"},
     {SupportedLanguage::kBn, "bn"},          {SupportedLanguage::kDe, "de"},
     {SupportedLanguage::kFr, "fr"},          {SupportedLanguage::kHi, "hi"},
     {SupportedLanguage::kIt, "it"},          {SupportedLanguage::kKo, "ko"},
     {SupportedLanguage::kNl, "nl"},          {SupportedLanguage::kPl, "pl"},
     {SupportedLanguage::kPt, "pt"},          {SupportedLanguage::kRu, "ru"},
     {SupportedLanguage::kTh, "th"},          {SupportedLanguage::kTr, "tr"},
     {SupportedLanguage::kVi, "vi"},          {SupportedLanguage::kZh, "zh"},
     {SupportedLanguage::kZhHant, "zh-Hant"}, {SupportedLanguage::kBg, "bg"},
     {SupportedLanguage::kCs, "cs"},          {SupportedLanguage::kDa, "da"},
     {SupportedLanguage::kEl, "el"},          {SupportedLanguage::kFi, "fi"},
     {SupportedLanguage::kHr, "hr"},          {SupportedLanguage::kHu, "hu"},
     {SupportedLanguage::kId, "id"},          {SupportedLanguage::kIw, "iw"},
     {SupportedLanguage::kLt, "lt"},          {SupportedLanguage::kNo, "no"},
     {SupportedLanguage::kRo, "ro"},          {SupportedLanguage::kSk, "sk"},
     {SupportedLanguage::kSl, "sl"},          {SupportedLanguage::kSv, "sv"},
     {SupportedLanguage::kUk, "uk"},          {SupportedLanguage::kKn, "kn"},
     {SupportedLanguage::kTa, "ta"},          {SupportedLanguage::kTe, "te"},
     {SupportedLanguage::kMr, "mr"}});
static_assert(std::size(kSupportedLanguageCodeMap) ==
                  static_cast<unsigned>(SupportedLanguage::kMaxValue) + 1,
              "All languages must be in kSupportedLanguageCodeMap.");

// The inverse of kSupportedLanguageCodeMap.
inline constexpr auto kSupportedLanguageCodeInverseMap =
    base::MakeFixedFlatMap<std::string_view, SupportedLanguage>(
        kSupportedLanguageCodeMap,
        /*comp=*/{},
        [](const auto& entry) { return std::pair(entry.second, entry.first); });
}  // namespace

std::pair<SupportedLanguage, SupportedLanguage>
SupportedLanguagePairFromNonEnglishSupportedLanguage(
    SupportedLanguage supported_language) {
  CHECK_NE(supported_language, SupportedLanguage::kEn);
  if (kSupportedLanguageCodeMap.at(supported_language) < "en") {
    return std::make_pair(supported_language, SupportedLanguage::kEn);
  } else {
    return std::make_pair(SupportedLanguage::kEn, supported_language);
  }
}

// Converts a SupportedLanguage to a language code.
std::string_view ToLanguageCode(SupportedLanguage supported_language) {
  return kSupportedLanguageCodeMap.at(supported_language);
}

// Converts a language code to a SupportedLanguage.
std::optional<SupportedLanguage> ToSupportedLanguage(
    std::string_view language_code) {
  auto it = kSupportedLanguageCodeInverseMap.find(language_code);
  if (it != kSupportedLanguageCodeInverseMap.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace on_device_translation
