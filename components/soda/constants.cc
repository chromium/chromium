// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/constants.h"

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/crx_file/id_util.h"
#include "media/base/media_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace speech {
const constexpr char* const kDefaultEnabledLanguages[] = {"fr-FR", "it-IT",
                                                          "de-DE"};

const char kUsEnglishLocale[] = "en-US";

const char kEnglishLocaleNoCountry[] = "en";
const char kChineseLocaleNoCountry[] = "zh";

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
    const std::string& language) {
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

absl::optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    LanguageCode language_code) {
  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    if (config.language_code == language_code) {
      return config;
    }
  }

  return absl::nullopt;
}

absl::optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    const std::string& language_name) {
  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    if (base::ToLowerASCII(config.language_name) ==
        base::ToLowerASCII(language_name)) {
      return config;
    }
  }

  return absl::nullopt;
}

LanguageCode GetLanguageCodeByComponentId(const std::string& component_id) {
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
    absl::optional<SodaLanguagePackComponentConfig> language_config =
        GetLanguageComponentConfig(language_code);
    if (language_config.has_value()) {
      language_name = language_config.value().language_name;
    }
  }

  return language_name;
}

LanguageCode GetLanguageCode(const std::string& language_name) {
  absl::optional<SodaLanguagePackComponentConfig> language_config =
      GetLanguageComponentConfig(language_name);
  if (language_config.has_value()) {
    return language_config.value().language_code;
  }
  return LanguageCode::kNone;
}

const std::u16string GetLanguageDisplayName(const std::string& language_name,
                                            const std::string& display_locale) {
  if (language_name.substr(0, 2) == kChineseLocaleNoCountry) {
    return l10n_util::GetDisplayNameForLocale(language_name, display_locale,
                                              true);

  } else {
    return l10n_util::GetDisplayNameForLocaleWithoutCountry(
        language_name, display_locale, true);
  }
}

const std::string GetInstallationSuccessTimeMetricForLanguagePack(
    const LanguageCode& language_code) {
  auto config = GetLanguageComponentConfig(language_code);
  DCHECK(config && config->language_name);
  return base::StrCat({"SodaInstaller.Language.", config->language_name,
                       ".InstallationSuccessTime"});
}

const std::string GetInstallationFailureTimeMetricForLanguagePack(
    const LanguageCode& language_code) {
  auto config = GetLanguageComponentConfig(language_code);
  DCHECK(config && config->language_name);
  return base::StrCat({"SodaInstaller.Language.", config->language_name,
                       ".InstallationFailureTime"});
}

const std::string GetInstallationResultMetricForLanguagePack(
    const LanguageCode& language_code) {
  auto config = GetLanguageComponentConfig(language_code);
  DCHECK(config && config->language_name);
  return base::StrCat({"SodaInstaller.Language.", config->language_name,
                       ".InstallationResult"});
}

std::vector<std::string> GetEnabledLanguages() {
  std::vector<std::string> enabled_languages = base::SplitString(
      base::GetFieldTrialParamValueByFeature(
          media::kLiveCaptionExperimentalLanguages, "available_languages"),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  for (const char* const enabled_language : kDefaultEnabledLanguages) {
    enabled_languages.push_back(enabled_language);
  }

  return enabled_languages;
}

}  // namespace speech
