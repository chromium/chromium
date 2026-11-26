// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/translation_manager_util.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/legacy_language_tag_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/metrics.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/l10n/l10n_util.h"

namespace on_device_translation {

namespace {

constexpr char kCanTranslateMetricActionName[] = "CanTranslate";
constexpr char kCanTranslateSourceLanguageIsSupportedMetricName[] =
    "Translate.OnDeviceTranslation.CanTranslate.IsSourceLanguageSupported";
constexpr char kCanTranslateTargetLanguageIsSupportedMetricName[] =
    "Translate.OnDeviceTranslation.CanTranslate.IsTargetLanguageSupported";

OnDeviceTranslationController::CanTranslateResult ToCanTranslateResult(
    InstallError error) {
  switch (error) {
    case InstallError::kNotEnoughDiskSpace:
      return OnDeviceTranslationController::CanTranslateResult::
          kNoNotEnoughDiskSpace;
    case InstallError::kDataSaverEnabled:
      return OnDeviceTranslationController::CanTranslateResult::
          kNoDataSaverEnabled;
    case InstallError::kMeteredConnectionBlocked:
      return OnDeviceTranslationController::CanTranslateResult::
          kNoMeteredConnectionBlocked;
    case InstallError::kFailed:
    case InstallError::kOffline:
      NOTREACHED();
  }
}

void CanTranslateImpl(
    OnDeviceTranslationInstaller& installer,
    const std::string& source_lang,
    const std::string& target_lang,
    OnDeviceTranslationController::CanTranslateCallback callback) {
  using CanTranslateResult = OnDeviceTranslationController::CanTranslateResult;
  // Get information on the registration and install status of the language
  // packs required for translation.
  LanguagePackRequirements language_pack_requirements =
      GetLanguagePackRequirements(installer, source_lang, target_lang);

  if (language_pack_requirements.required_packs.empty()) {
    // Empty `required_packs` means that the translation for the specified
    // language pair is not supported.
    std::move(callback).Run(CanTranslateResult::kNoNotSupportedLanguage);
    return;
  }

  if (!installer.IsSupported()) {
    std::move(callback).Run(CanTranslateResult::kNoUnsupportedDevice);
    return;
  }

  const bool needs_library = !installer.IsInit();
  const bool needs_language_packs =
      !language_pack_requirements.required_not_installed_packs.empty();

  CanTranslateResult ready_or_download_result;
  if (!needs_language_packs) {
    ready_or_download_result =
        needs_library ? CanTranslateResult::kAfterDownloadLibraryNotReady
                      : CanTranslateResult::kReadily;
  } else {
    ready_or_download_result =
        needs_library
            ? CanTranslateResult::kAfterDownloadLibraryAndLanguagePackNotReady
            : CanTranslateResult::kAfterDownloadLanguagePackNotReady;
  }

  if (!needs_library && !needs_language_packs) {
    std::move(callback).Run(ready_or_download_result);
    return;
  }

  installer.CheckDownloadEligibility(
      needs_library, needs_language_packs,
      base::BindOnce(
          [](CanTranslateResult ready_or_download_result,
             OnDeviceTranslationController::CanTranslateCallback callback,
             std::optional<InstallError> error) {
            std::move(callback).Run(error.has_value()
                                        ? ToCanTranslateResult(*error)
                                        : ready_or_download_result);
          },
          ready_or_download_result, std::move(callback)));
}

// TODO(crbug.com/419848973): This is a workaround until the "he" language code
// is fully supported.
std::string SwitchLanguageCodeToIwIfHe(std::string_view language_code) {
  if (language_code == "he") {
    return "iw";
  }
  auto split = base::SplitStringOnce(language_code, "-");
  if (!split || split->first != "he") {
    return std::string(language_code);
  }

  return base::StrCat({"iw-", split->second});
}

std::string SwitchLanguageCodeForChinese(std::string_view language_code) {
  std::string lower_code = base::ToLowerASCII(language_code);
  if (lower_code == "zh-cn" || lower_code == "zh-hans-cn" ||
      lower_code == "cmn-hans-cn") {
    return "zh";
  }
  if (lower_code == "zh-tw" || lower_code == "zh-hant-tw" ||
      lower_code == "cmn-hant-tw" || lower_code == "zh-hk" ||
      lower_code == "zh-hant-hk" || lower_code == "cmn-hant-hk") {
    return "zh-Hant";
  }
  return std::string(language_code);
}

}  // namespace

const std::vector<std::string_view> GetAcceptLanguages(
    content::BrowserContext* browser_context) {
  CHECK(browser_context);

  PrefService* profile_pref = user_prefs::UserPrefs::Get(browser_context);
  const std::vector<std::string_view> accept_languages = base::SplitStringPiece(
      profile_pref->GetString(language::prefs::kAcceptLanguages), ",",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  return accept_languages;
}

bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string_view lang) {
  return std::ranges::contains(
      accept_languages, base::i18n::GetLanguageSubtagUsingLanguageTag(lang));
}

bool IsTranslatorAllowed(content::BrowserContext* browser_context) {
  CHECK(browser_context);
  return user_prefs::UserPrefs::Get(browser_context)
      ->GetBoolean(prefs::kTranslatorAPIAllowed);
}

std::optional<std::string> GetBestFitLanguageCode(
    std::string_view language_code) {
  std::string best_fit = SwitchLanguageCodeToIwIfHe(language_code);
  best_fit = SwitchLanguageCodeForChinese(best_fit);
  return LookupMatchingLocaleByBestFit(kSupportedLanguageCodes, best_fit);
}

LanguagePackRequirements GetLanguagePackRequirements(
    const OnDeviceTranslationInstaller& installer,
    const std::string& source_lang,
    const std::string& target_lang) {
  LanguagePackRequirements language_pack_requirements;

  // Calculate required language packs.
  language_pack_requirements.required_packs =
      CalculateRequiredLanguagePacks(source_lang, target_lang);

  // Calculate required, not installed language packs.
  const auto installed_packs = installer.InstalledLanguagePacks();
  std::ranges::set_difference(
      language_pack_requirements.required_packs, installed_packs,
      std::back_inserter(
          language_pack_requirements.required_not_installed_packs));

  // Calculate to be registered language packs.
  const auto registered_packs = installer.RegisteredLanguagePacks();
  std::ranges::set_difference(
      language_pack_requirements.required_not_installed_packs, registered_packs,
      std::back_inserter(language_pack_requirements.to_be_registered_packs));

  return language_pack_requirements;
}

void CanTranslateWithInstaller(
    OnDeviceTranslationInstaller& installer,
    const std::string& source_lang_arg,
    const std::string& target_lang_arg,
    OnDeviceTranslationController::CanTranslateCallback callback) {
  std::optional<std::string> best_fit_source_language =
      GetBestFitLanguageCode(source_lang_arg);
  std::optional<std::string> best_fit_target_language =
      GetBestFitLanguageCode(target_lang_arg);
  base::UmaHistogramBoolean(kCanTranslateSourceLanguageIsSupportedMetricName,
                            best_fit_source_language.has_value());
  base::UmaHistogramBoolean(kCanTranslateTargetLanguageIsSupportedMetricName,
                            best_fit_target_language.has_value());
  if (!best_fit_source_language.has_value() ||
      !best_fit_target_language.has_value()) {
    std::move(callback).Run(OnDeviceTranslationController::CanTranslateResult::
                                kNoNotSupportedLanguage);
    return;
  }

  RecordOnDeviceTranslationCallForLanguagePair(kCanTranslateMetricActionName,
                                               *best_fit_source_language,
                                               *best_fit_target_language);
  CanTranslateImpl(installer, *best_fit_source_language,
                   *best_fit_target_language, std::move(callback));
}

}  // namespace on_device_translation
