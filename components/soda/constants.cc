// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/constants.h"

#include <optional>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/i18n/chinese_helpers.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/legacy_language_tag_helpers.h"
#include "base/i18n/tag_converters.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/crx_file/id_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace speech {
namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::GetLanguageTagFromString;
using ::base::i18n::LanguageTag;

constexpr auto kChineseLocaleMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{"cmn-hans-cn", "cmn-Hans-CN"},
         {"cmn-hant-tw", "cmn-Hant-TW"},
         {"zh-cn", "cmn-Hans-CN"},
         {"zh-hans-cn", "cmn-Hans-CN"},
         {"zh-hant-tw", "cmn-Hant-TW"},
         {"zh-tw", "cmn-Hant-TW"}});

}  // namespace

// If `language_name` is Chinese variant, then return the master locale.
// Otherwise, return `language_name`.
const std::string MaybeMapToChineseLocale(std::string_view language_name) {
  auto chinese_locale =
      kChineseLocaleMap.find(base::ToLowerASCII(language_name));
  if (chinese_locale != kChineseLocaleMap.end()) {
    return std::string(chinese_locale->second);
  }

  return std::string(language_name);
}

std::optional<LanguageTag> GetLanguageTagFromSodaLanguage(
    std::string_view soda_language) {
  std::optional<LanguageTag> soda_language_tag =
      GetLanguageTagFromString(soda_language);
  if (!soda_language_tag) {
    return std::nullopt;
  }
  if (!base::i18n::IsChinese(*soda_language_tag)) {
    return *soda_language_tag;
  }
  return base::i18n::IsTraditionalChinese(*soda_language_tag)
             ? GetKnownLanguageTag("zh-Hant")
             : GetKnownLanguageTag("zh");
}

const char kUsEnglishLocale[] = "en-US";

const char kEnglishLocaleNoCountry[] = "en";
const char kChineseLocaleNoCountry[] = "cmn";

const char kSodaPreemptiveDownloadStarted[] =
    "SodaInstaller.PreemptiveDownloadStarted";

const char kSodaBinaryInstallationResult[] =
    "SodaInstaller.BinaryInstallationResult";

const char kSodaBinaryInstallationSuccessTimeTaken[] =
    "SodaInstaller.BinaryInstallationSuccessTime";

const char kSodaBinaryInstallationFailureTimeTaken[] =
    "SodaInstaller.BinaryInstallationFailureTime";

#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kSodaBinaryRelativePath[] =
    FILE_PATH_LITERAL("SODAFiles/SODA.dll");
#else
constexpr base::FilePath::CharType kSodaBinaryRelativePath[] =
    FILE_PATH_LITERAL("SODAFiles/libsoda.so");
#endif

constexpr base::FilePath::CharType kSodaTestBinaryRelativePath[] =
    FILE_PATH_LITERAL("libsoda.so");

constexpr base::FilePath::CharType kSodaTestResourcesRelativePath[] =
    FILE_PATH_LITERAL("third_party/soda/resources/");

constexpr base::FilePath::CharType kSodaInstallationRelativePath[] =
    FILE_PATH_LITERAL("SODA");

constexpr base::FilePath::CharType kSodaLanguagePacksRelativePath[] =
    FILE_PATH_LITERAL("SODALanguagePacks");

constexpr base::FilePath::CharType kSodaLanguagePackDirectoryRelativePath[] =
    FILE_PATH_LITERAL("SODAModels");

const base::FilePath GetSodaDirectory() {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);

  return components_dir.empty()
             ? base::FilePath()
             : components_dir.Append(kSodaInstallationRelativePath);
}

const base::FilePath GetSodaLanguagePacksDirectory() {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);

  return components_dir.empty()
             ? base::FilePath()
             : components_dir.Append(kSodaLanguagePacksRelativePath);
}

const base::FilePath GetSodaTestResourcesDirectory() {
  base::FilePath test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_root);
  DCHECK(!test_data_root.empty());
  return test_data_root.empty()
             ? base::FilePath()
             : test_data_root.Append(kSodaTestResourcesRelativePath);
}

const base::FilePath GetLatestSodaLanguagePackDirectory(
    std::string_view language) {
  base::FileEnumerator enumerator(
      GetSodaLanguagePacksDirectory().AppendASCII(language), false,
      base::FileEnumerator::DIRECTORIES);

  // Use the lexographical order of the directory names to determine the latest
  // version. This mirrors the logic in the component updater.
  base::FilePath latest_version_dir;
  for (base::FilePath version_dir = enumerator.Next(); !version_dir.empty();
       version_dir = enumerator.Next()) {
    latest_version_dir =
        latest_version_dir < version_dir ? version_dir : latest_version_dir;
  }

  return latest_version_dir.Append(kSodaLanguagePackDirectoryRelativePath);
}

const base::FilePath GetLatestSodaDirectory() {
  base::FileEnumerator enumerator(GetSodaDirectory(), false,
                                  base::FileEnumerator::DIRECTORIES);
  base::FilePath latest_version_dir;
  for (base::FilePath version_dir = enumerator.Next(); !version_dir.empty();
       version_dir = enumerator.Next()) {
    latest_version_dir =
        latest_version_dir < version_dir ? version_dir : latest_version_dir;
  }

  return latest_version_dir;
}

const base::FilePath GetSodaBinaryPath() {
  base::FilePath soda_dir = GetLatestSodaDirectory();
  return soda_dir.empty() ? base::FilePath()
                          : soda_dir.Append(kSodaBinaryRelativePath);
}

const base::FilePath GetSodaTestBinaryPath() {
  base::FilePath test_dir = GetSodaTestResourcesDirectory();
  return test_dir.empty() ? base::FilePath()
                          : test_dir.Append(kSodaTestBinaryRelativePath);
}

std::optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    LanguageCode language_code) {
  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    if (config.language_code == language_code) {
      return config;
    }
  }

  return std::nullopt;
}

std::optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    std::string_view language_name) {
  auto locale = MaybeMapToChineseLocale(language_name);
  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    if (base::EqualsCaseInsensitiveASCII(config.language_name, locale)) {
      return config;
    }
  }

  return std::nullopt;
}

std::optional<SodaLanguagePackComponentConfig>
GetLanguageComponentConfigMatchingLanguageSubtag(
    std::string_view language_name) {
  // Use full locale to get Chinese variant config.
  auto locale = MaybeMapToChineseLocale(language_name);
  if (locale.substr(0, 3) == kChineseLocaleNoCountry) {
    return GetLanguageComponentConfig(locale);
  }

  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    if (base::i18n::GetLanguageSubtagUsingLanguageTag(
            base::ToLowerASCII(config.language_name)) ==
        base::i18n::GetLanguageSubtagUsingLanguageTag(
            base::ToLowerASCII(language_name))) {
      return config;
    }
  }

  return std::nullopt;
}

LanguageCode GetLanguageCodeByComponentId(std::string_view component_id) {
  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    if (crx_file::id_util::GenerateIdFromHash(config.public_key_sha) ==
        component_id) {
      return config.language_code;
    }
  }

  return LanguageCode::kNone;
}

std::string GetLanguageName(LanguageCode language_code) {
  std::string language_name;
  if (language_code != LanguageCode::kNone) {
    std::optional<SodaLanguagePackComponentConfig> language_config =
        GetLanguageComponentConfig(language_code);
    if (language_config.has_value()) {
      language_name = language_config.value().language_name;
    }
  }

  return language_name;
}

LanguageCode GetLanguageCode(std::string_view language_name) {
  std::optional<SodaLanguagePackComponentConfig> language_config =
      GetLanguageComponentConfig(language_name);
  if (language_config.has_value()) {
    return language_config.value().language_code;
  }
  return LanguageCode::kNone;
}

const std::u16string GetLanguageDisplayName(std::string_view language_name,
                                            std::string_view display_locale) {
  std::optional<LanguageTag> language_tag_name =
      GetLanguageTagFromString(language_name);
  std::optional<LanguageTag> display_locale_tag =
      GetLanguageTagFromString(display_locale);
  if (!language_tag_name || !display_locale_tag) {
    return std::u16string();
  }

  return l10n_util::GetDisplayNameForLocale(
      base::i18n::IsChinese(*language_tag_name)
          ? *language_tag_name
          : language_tag_name->WithLanguageSubtagOnly(),
      *display_locale_tag, true);
}

const std::string GetInstallationSuccessTimeMetricForLanguagePack(
    const LanguageCode& language_code) {
  auto config = GetLanguageComponentConfig(language_code);
  DCHECK(config && !config->language_name.empty());
  return GetInstallationSuccessTimeMetricForLanguage(config->language_name);
}
const std::string GetInstallationSuccessTimeMetricForLanguage(
    std::string_view language) {
  return base::StrCat(
      {"SodaInstaller.Language.", language, ".InstallationSuccessTime"});
}

const std::string GetInstallationFailureTimeMetricForLanguagePack(
    const LanguageCode& language_code) {
  auto config = GetLanguageComponentConfig(language_code);
  DCHECK(config && !config->language_name.empty());
  return GetInstallationFailureTimeMetricForLanguage(config->language_name);
}

const std::string GetInstallationFailureTimeMetricForLanguage(
    std::string_view language) {
  return base::StrCat(
      {"SodaInstaller.Language.", language, ".InstallationFailureTime"});
}

const std::string GetInstallationResultMetricForLanguagePack(
    const LanguageCode& language_code) {
  auto config = GetLanguageComponentConfig(language_code);
  DCHECK(config && !config->language_name.empty());
  return speech::GetInstallationResultMetricForLanguage(config->language_name);
}

const std::string GetInstallationResultMetricForLanguage(
    std::string_view language) {
  return base::StrCat(
      {"SodaInstaller.Language.", language, ".InstallationResult"});
}

const std::string GetUninstalledDueToExpirationMetricForLanguage(
    std::string_view language) {
  return base::StrCat(
      {"SodaInstaller.Language.", language, ".UninstalledDueToExpiration"});
}

const std::string GetRedownloadedAfterExpirationMetricForLanguage(
    std::string_view language) {
  return base::StrCat(
      {"SodaInstaller.Language.", language, ".RedownloadedAfterExpiration"});
}

std::string_view GetDefaultLiveCaptionLanguage(
    std::string_view application_locale,
    const PrefService& profile_prefs) {
  std::optional<SodaLanguagePackComponentConfig> application_locale_config =
      GetLanguageComponentConfigMatchingLanguageSubtag(application_locale);

  if (application_locale_config.has_value() &&
      application_locale_config.value().language_code != LanguageCode::kNone) {
    return application_locale_config.value().language_name;
  }

  std::string accept_languages_pref =
      profile_prefs.GetString(language::prefs::kAcceptLanguages);
  for (std::string language :
       base::SplitString(accept_languages_pref, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    std::optional<SodaLanguagePackComponentConfig> config =
        GetLanguageComponentConfigMatchingLanguageSubtag(language);
    if (config.has_value() &&
        config.value().language_code != LanguageCode::kNone) {
      return config.value().language_name;
    }
  }

  return kUsEnglishLocale;
}

}  // namespace speech
