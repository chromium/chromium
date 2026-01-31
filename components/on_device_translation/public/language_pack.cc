// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/public/language_pack.h"

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "components/on_device_translation/public/supported_languages.h"

namespace on_device_translation {
namespace {

constexpr char kPrefNamePrefix[] =
    "on_device_translation.translate_kit_packages.";
constexpr char kComponentPathPrefNameSuffix[] = "_path";
constexpr char kRegisteredFlagPrefNameSuffix[] = "_registered";

// Currently we always translate via English, so the number of
// SupportedLanguages needs to include English in addition to all the
// LanguagePackKeys.
static_assert(static_cast<unsigned>(SupportedLanguage::kMaxValue) ==
                  static_cast<unsigned>(LanguagePackKey::kMaxValue) + 1,
              "Missmatching SupportedLanguage size and LanguagePackKey size");

LanguagePackKey LanguagePackKeyFromNonEnglishSupportedLanguage(
    SupportedLanguage supported_language) {
  CHECK_NE(supported_language, SupportedLanguage::kEn);
  return static_cast<LanguagePackKey>(
      static_cast<unsigned>(supported_language) - 1);
}

}  // namespace

LanguagePackRequirements::LanguagePackRequirements() = default;
LanguagePackRequirements::~LanguagePackRequirements() = default;
LanguagePackRequirements::LanguagePackRequirements(
    LanguagePackRequirements&&) noexcept = default;
LanguagePackRequirements& LanguagePackRequirements::operator=(
    LanguagePackRequirements&&) noexcept = default;

SupportedLanguage NonEnglishSupportedLanguageFromLanguagePackKey(
    LanguagePackKey language_pack_key) {
  return static_cast<SupportedLanguage>(
      static_cast<unsigned>(language_pack_key) + 1);
}

const LanguagePackComponentConfig& GetLanguagePackComponentConfig(
    LanguagePackKey key) {
  return *kLanguagePackComponentConfigMap.at(key);
}

// Calculates the required language packs for a translation from source_lang to
// target_lang.
// Note: Currently, this method is implemented assuming that translation between
// non-English languages is done by first translating to English. This logic
// needs to be updated when direct translation between non-English languages is
// supported by the library.
std::set<LanguagePackKey> CalculateRequiredLanguagePacks(
    const std::string& source_lang,
    const std::string& target_lang) {
  auto source_lang_code = ToSupportedLanguage(source_lang);
  auto target_lang_code = ToSupportedLanguage(target_lang);
  if (!source_lang_code.has_value() || !target_lang_code.has_value() ||
      source_lang_code == target_lang_code) {
    return {};
  }
  if (*source_lang_code == SupportedLanguage::kEn) {
    return {LanguagePackKeyFromNonEnglishSupportedLanguage(*target_lang_code)};
  }
  if (*target_lang_code == SupportedLanguage::kEn) {
    return {LanguagePackKeyFromNonEnglishSupportedLanguage(*source_lang_code)};
  }
  return {LanguagePackKeyFromNonEnglishSupportedLanguage(*source_lang_code),
          LanguagePackKeyFromNonEnglishSupportedLanguage(*target_lang_code)};
}

std::string GetPackageInstallDirName(LanguagePackKey language_pack_key) {
  const auto supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto lang_pair =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return base::StrCat(
      {ToLanguageCode(lang_pair.first), "_", ToLanguageCode(lang_pair.second)});
}

std::string GetPackageNameSuffix(LanguagePackKey language_pack_key) {
  const auto supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto lang_pair =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return base::StrCat(
      {ToLanguageCode(lang_pair.first), "-", ToLanguageCode(lang_pair.second)});
}

std::vector<std::string> GetPackageInstallSubDirNamesForVerification(
    LanguagePackKey language_pack_key) {
  const auto supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto lang_pair =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return {
      base::StrCat({ToLanguageCode(lang_pair.first), "_",
                    ToLanguageCode(lang_pair.second), "_dictionary"}),
      base::StrCat({ToLanguageCode(lang_pair.first), "_",
                    ToLanguageCode(lang_pair.second), "_nmt"}),
      base::StrCat({ToLanguageCode(lang_pair.second), "_",
                    ToLanguageCode(lang_pair.first), "_nmt"}),
  };
}

std::string_view GetSourceLanguageCode(LanguagePackKey language_pack_key) {
  const SupportedLanguage supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto [source_lang, _] =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return ToLanguageCode(source_lang);
}

std::string_view GetTargetLanguageCode(LanguagePackKey language_pack_key) {
  const SupportedLanguage supported_language =
      NonEnglishSupportedLanguageFromLanguagePackKey(language_pack_key);
  const auto [_, target_lang] =
      SupportedLanguagePairFromNonEnglishSupportedLanguage(supported_language);
  return ToLanguageCode(target_lang);
}

std::string GetComponentPathPrefName(
    const LanguagePackComponentConfig& config) {
  return base::StrCat({kPrefNamePrefix, ToLanguageCode(config.language1), "_",
                       ToLanguageCode(config.language2),
                       kComponentPathPrefNameSuffix});
}

std::string GetRegisteredFlagPrefName(
    const LanguagePackComponentConfig& config) {
  return base::StrCat({kPrefNamePrefix, ToLanguageCode(config.language1), "_",
                       ToLanguageCode(config.language2),
                       kRegisteredFlagPrefNameSuffix});
}

}  // namespace on_device_translation
