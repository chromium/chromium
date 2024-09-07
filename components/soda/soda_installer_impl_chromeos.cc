// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer_impl_chromeos.h"

#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/pref_names.h"
#include "components/soda/soda_features.h"
#include "components/soda/soda_installer.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace speech {
namespace {

constexpr char kSodaDlcName[] = "libsoda";
constexpr char kSodaEnglishUsDlcName[] = "libsoda-model-en-us";

SodaInstaller::ErrorCode DlcCodeToSodaErrorCode(const std::string& code) {
  return (code == dlcservice::kErrorNeedReboot)
             ? SodaInstaller::ErrorCode::kNeedsReboot
             : SodaInstaller::ErrorCode::kUnspecifiedError;
}
const constexpr char* const kDefaultCrOSEnabledLanguages[] = {"da-DK", "nl-NL",
                                                              "nb-NO", "sv-SE"};

}  // namespace

SodaInstallerImplChromeOS::SodaInstallerImplChromeOS() {
  available_languages_ = ConstructAvailableLanguages();
}

void SodaInstallerImplChromeOS::InitLanguages(PrefService* profile_prefs,
                                              PrefService* global_prefs) {
  if (global_prefs->GetList(prefs::kSodaRegisteredLanguagePacks).empty()) {
    // TODO(crbug.com/1200667): Register the default language used by
    // Dictation on ChromeOS.
    std::string projector_language_code =
        profile_prefs->GetString(ash::prefs::kProjectorCreationFlowLanguage);
    RegisterLanguage(projector_language_code, global_prefs);

    RegisterLanguage(prefs::GetLiveCaptionLanguageCode(profile_prefs),
                     global_prefs);
  }

  for (const auto& language :
       global_prefs->GetList(prefs::kSodaRegisteredLanguagePacks)) {
    SodaInstaller::GetInstance()->InstallLanguage(language.GetString(),
                                                  global_prefs);
  }
}

base::flat_map<std::string, SodaInstallerImplChromeOS::LanguageInfo>
SodaInstallerImplChromeOS::ConstructAvailableLanguages() const {
  base::flat_map<std::string, LanguageInfo> available_languages;
  // Defaults checked in.
  if (!base::FeatureList::IsEnabled(kCrosExpandSodaLanguages)) {
    available_languages.insert(
        {kUsEnglishLocale, {kSodaEnglishUsDlcName, LanguageCode::kEnUs}});
    return available_languages;
  }
  available_languages.insert(
      {kUsEnglishLocale, {"libsoda-model-en-us-df24d1", LanguageCode::kEnUs}});
  available_languages.insert(
      {"ja-JP", {"libsoda-model-ja-jp-df24d1", LanguageCode::kJaJp}});
  available_languages.insert(
      {"de-DE", {"libsoda-model-de-de-df24d1", LanguageCode::kDeDe}});
  available_languages.insert(
      {"fr-FR", {"libsoda-model-fr-fr-df24d1", LanguageCode::kFrFr}});
  available_languages.insert(
      {"it-IT", {"libsoda-model-it-it-df24d1", LanguageCode::kItIt}});
  available_languages.insert(
      {"en-CA", {"libsoda-model-en-ca-df24d1", LanguageCode::kEnCa}});
  available_languages.insert(
      {"en-AU", {"libsoda-model-en-au-df24d1", LanguageCode::kEnAu}});
  available_languages.insert(
      {"en-GB", {"libsoda-model-en-gb-df24d1", LanguageCode::kEnGb}});
  available_languages.insert(
      {"en-IE", {"libsoda-model-en-ie-df24d1", LanguageCode::kEnIe}});
  available_languages.insert(
      {"en-SG", {"libsoda-model-en-sg-df24d1", LanguageCode::kEnSg}});
  available_languages.insert(
      {"fr-BE", {"libsoda-model-fr-be-df24d1", LanguageCode::kFrBe}});
  available_languages.insert(
      {"fr-CH", {"libsoda-model-fr-ch-df24d1", LanguageCode::kFrCh}});
  available_languages.insert(
      {"en-IN", {"libsoda-model-en-in-df24d1", LanguageCode::kEnIn}});
  available_languages.insert(
      {"it-CH", {"libsoda-model-it-ch-df24d1", LanguageCode::kItCh}});
  available_languages.insert(
      {"de-AT", {"libsoda-model-de-at-df24d1", LanguageCode::kDeAt}});
  available_languages.insert(
      {"de-BE", {"libsoda-model-de-be-df24d1", LanguageCode::kDeBe}});
  available_languages.insert(
      {"de-CH", {"libsoda-model-de-ch-df24d1", LanguageCode::kDeCh}});
  available_languages.insert(
      {"es-US", {"libsoda-model-es-us-df24d1", LanguageCode::kEsUs}});
  available_languages.insert(
      {"es-ES", {"libsoda-model-es-us-df24d1", LanguageCode::kEsEs}});
  available_languages.insert(
      {"fr-CA", {"libsoda-model-fr-ca-df24d1", LanguageCode::kFrCa}});
  available_languages.insert(
      {"hi-IN", {"libsoda-model-hi-in-df24d1", LanguageCode::kHiIn}});
  available_languages.insert(
      {"id-ID", {"libsoda-model-id-id-df24d1", LanguageCode::kIdId}});
  available_languages.insert(
      {"ko-KR", {"libsoda-model-ko-kr-df24d1", LanguageCode::kKoKr}});
  available_languages.insert(
      {"pl-PL", {"libsoda-model-pl-pl-df24d1", LanguageCode::kPlPl}});
  available_languages.insert(
      {"th-TH", {"libsoda-model-th-th-df24d1", LanguageCode::kThTh}});
  available_languages.insert(
      {"tr-TR", {"libsoda-model-tr-tr-df24d1", LanguageCode::kTrTr}});
  available_languages.insert(
      {"cmn-Hant-TW", {"libsoda-model-zh-tw-df24d1", LanguageCode::kZhTw}});
  available_languages.insert(
      {"cmn-Hans-CN", {"libsoda-model-zh-cn-df24d1", LanguageCode::kZhCn}});
  available_languages.insert(
      {"pt-BR", {"libsoda-model-pt-br-df24d1", LanguageCode::kPtBr}});
  available_languages.insert(
      {"ru-RU", {"libsoda-model-ru-ru-df24d1", LanguageCode::kRuRu}});
  available_languages.insert(
      {"vi-VN", {"libsoda-model-vi-vn-df24d1", LanguageCode::kViVn}});
  available_languages.insert({"da-DK", {"", LanguageCode::kDaDk}});
  available_languages.insert({"nb-NO", {"", LanguageCode::kNbNo}});
  available_languages.insert({"nl-NL", {"", LanguageCode::kNlNl}});
  available_languages.insert({"sv-SE", {"", LanguageCode::kSvSe}});

  if (base::FeatureList::IsEnabled(kFeatureManagementCrosSodaConchLanguages) &&
      base::FeatureList::IsEnabled(kCrosSodaConchLanguages)) {
    available_languages["da-DK"] = {"libsoda-model-da-dk-cnch24d2",
                                    LanguageCode::kDaDk};
    available_languages["de-AT"] = {"libsoda-model-de-at-cnch24d2",
                                    LanguageCode::kDeAt};
    available_languages["de-BE"] = {"libsoda-model-de-be-cnch24d2",
                                    LanguageCode::kDeBe};
    available_languages["de-CH"] = {"libsoda-model-de-ch-cnch24d2",
                                    LanguageCode::kDeCh};
    available_languages["de-DE"] = {"libsoda-model-de-de-cnch24d2",
                                    LanguageCode::kDeDe};
    available_languages["en-AU"] = {"libsoda-model-en-au-cnch24d2",
                                    LanguageCode::kEnAu};
    available_languages["en-CA"] = {"libsoda-model-en-ca-cnch24d2",
                                    LanguageCode::kEnCa};
    available_languages["en-GB"] = {"libsoda-model-en-gb-cnch24d2",
                                    LanguageCode::kEnGb};
    available_languages["en-IE"] = {"libsoda-model-en-ie-cnch24d2",
                                    LanguageCode::kEnIe};
    available_languages["en-IN"] = {"libsoda-model-en-in-cnch24d2",
                                    LanguageCode::kEnIn};
    available_languages["en-SG"] = {"libsoda-model-en-sg-cnch24d2",
                                    LanguageCode::kEnSg};
    available_languages["en-US"] = {"libsoda-model-en-us-cnch24d2",
                                    LanguageCode::kEnUs};
    available_languages["es-ES"] = {"libsoda-model-es-es-cnch24d2",
                                    LanguageCode::kEsEs};
    available_languages["es-US"] = {"libsoda-model-es-us-cnch24d2",
                                    LanguageCode::kEsUs};
    available_languages["fr-BE"] = {"libsoda-model-fr-be-cnch24d2",
                                    LanguageCode::kFrBe};
    available_languages["fr-CA"] = {"libsoda-model-fr-ca-cnch24d2",
                                    LanguageCode::kFrCa};
    available_languages["fr-CH"] = {"libsoda-model-fr-Ch-cnch24d2",
                                    LanguageCode::kFrCh};
    available_languages["fr-FR"] = {"libsoda-model-fr-fr-cnch24d2",
                                    LanguageCode::kFrFr};
    available_languages["hi-IN"] = {"libsoda-model-hi-in-cnch24d2",
                                    LanguageCode::kHiIn};
    available_languages["it-IT"] = {"libsoda-model-it-it-cnch24d2",
                                    LanguageCode::kItIt};
    available_languages["ja-JP"] = {"libsoda-model-ja-jp-cnch24d2",
                                    LanguageCode::kJaJp};
    available_languages["ko-KR"] = {"libsoda-model-ko-kr-cnch24d2",
                                    LanguageCode::kKoKr};
    available_languages["nb-NO"] = {"libsoda-model-nb-no-cnch24d2",
                                    LanguageCode::kNbNo};
    available_languages["nl-NL"] = {"libsoda-model-nl-nl-cnch24d2",
                                    LanguageCode::kNlNl};
    available_languages["sv-SE"] = {"libsoda-model-sv-se-cnch24d2",
                                    LanguageCode::kSvSe};
  }

  // Add in from feature flags. the value is of the format:
  // "en-AU:libsoda-modelname,fr-CA:,de-CH:libsoda-pizzaface,"
  // Note that fr-CA is removed explicitly in example.
  std::vector<std::string> langs =
      base::SplitString(base::GetFieldTrialParamValueByFeature(
                            kCrosExpandSodaLanguages, "available_languages"),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& unparsed_pair : langs) {
    std::vector<std::string> lang_model_pair = base::SplitString(
        unparsed_pair, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (lang_model_pair.size() != 2) {
      // skip, but log.
      LOG(DFATAL) << "Unable to parse a language pair, in wrong format. "
                     "value received and ignored is "
                  << unparsed_pair;
      continue;
    }
    if (lang_model_pair[1].rfind("libsoda", 0) == std::string::npos &&
        !lang_model_pair[1].empty()) {
      LOG(ERROR) << "Incorrect prefix for " << lang_model_pair[0]
                 << " given, is: " << lang_model_pair[1] << " and ignoring.";
      continue;
    }
    const auto& lang_it = available_languages.find(lang_model_pair[0]);
    if (lang_it == available_languages.end()) {
      LOG(ERROR) << "Unable to find language " << lang_model_pair[0]
                 << ", ignoring.";
      continue;
    }
    lang_it->second.dlc_name = lang_model_pair[1];
  }

  // Remove empty.
  base::EraseIf(available_languages,
                [](const auto& it) { return it.second.dlc_name.empty(); });
  return available_languages;
}

SodaInstallerImplChromeOS::~SodaInstallerImplChromeOS() = default;

base::FilePath SodaInstallerImplChromeOS::GetSodaBinaryPath() const {
  return soda_lib_path_;
}

base::FilePath SodaInstallerImplChromeOS::GetLanguagePath(
    const std::string& language) const {
  auto available_it = available_languages_.find(language);
  if (available_it == available_languages_.end()) {
    LOG(DFATAL) << "Asked for unavailable language " << language;
    return base::FilePath();
  }
  auto it = installed_language_paths_.find(available_it->second.language_code);
  if (it == installed_language_paths_.end()) {
    return base::FilePath();
  }
  return it->second;
}

void SodaInstallerImplChromeOS::InstallSoda(PrefService* global_prefs) {
  if (soda_binary_installed_ || never_download_soda_for_testing_) {
    return;
  }

  // Clear cached path in case this is a reinstallation (path could
  // change).
  SetSodaBinaryPath(base::FilePath());

  is_soda_downloading_ = true;
  soda_progress_ = 0.0;

  // Install SODA DLC.
  dlcservice::InstallRequest install_request;
  install_request.set_id(kSodaDlcName);
  ash::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&SodaInstallerImplChromeOS::OnSodaInstalled,
                     base::Unretained(this), base::Time::Now()),
      base::BindRepeating(&SodaInstallerImplChromeOS::OnSodaProgress,
                          base::Unretained(this)));
}

void SodaInstallerImplChromeOS::InstallLanguage(const std::string& language,
                                                PrefService* global_prefs) {
  if (never_download_soda_for_testing_)
    return;
  SodaInstaller::RegisterLanguage(language, global_prefs);
  // Clear cached path in case this is a reinstallation (path could
  // change).

  auto language_info = available_languages_.find(language);
  if (language_info == available_languages_.end()) {
    LOG(DFATAL)
        << "Language " << language
        << " not in list of available languages. refusing to install anything.";
    return;
  }

  SetLanguagePath(language_info->second.language_code, base::FilePath());
  language_pack_progress_.insert({language_info->second.language_code, 0.0});

  dlcservice::InstallRequest install_request;
  install_request.set_id(language_info->second.dlc_name);
  ash::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&SodaInstallerImplChromeOS::OnLanguageInstalled,
                     base::Unretained(this),
                     language_info->second.language_code, language,
                     base::Time::Now()),
      base::BindRepeating(&SodaInstallerImplChromeOS::OnLanguageProgress,
                          base::Unretained(this),
                          language_info->second.language_code));
}

void SodaInstallerImplChromeOS::UninstallLanguage(const std::string& language,
                                                  PrefService* global_prefs) {
  SodaInstaller::UnregisterLanguage(language, global_prefs);
  const auto& language_info = available_languages_.find(language);
  if (language_info == available_languages_.end()) {
    LOG(FATAL) << "Unable to uninstall language " << language
               << " as it is not in the list of available languages.";
  }
  const auto& dlc_name = language_info->second.dlc_name;
  installed_languages_.erase(language_info->second.language_code);
  installed_language_paths_.erase(language_info->second.language_code);
  language_pack_progress_.erase(language_info->second.language_code);

  ash::DlcserviceClient::Get()->Uninstall(
      dlc_name, base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                               base::Unretained(this), dlc_name));
}

std::vector<std::string> SodaInstallerImplChromeOS::GetAvailableLanguages()
    const {
  std::vector<std::string> languages;
  for (const auto& it : available_languages_) {
    languages.push_back(it.first);
  }
  return languages;
}

std::vector<std::string>
SodaInstallerImplChromeOS::GetLiveCaptionEnabledLanguages() const {
  auto enabled_languages = SodaInstaller::GetLiveCaptionEnabledLanguages();
  // extra CrOS languages.
  if (base::FeatureList::IsEnabled(kFeatureManagementCrosSodaConchLanguages) &&
      base::FeatureList::IsEnabled(kCrosSodaConchLanguages)) {
    for (const char* const enabled_language : kDefaultCrOSEnabledLanguages) {
      enabled_languages.push_back(enabled_language);
    }
  }
  return enabled_languages;
}

std::string SodaInstallerImplChromeOS::GetLanguageDlcNameForLocale(
    const std::string& locale) const {
  const auto& language_info = available_languages_.find(locale);
  if (language_info == available_languages_.end()) {
    LOG(DFATAL) << "Asked for unavailable language " << locale;
    return std::string();
  }

  return language_info->second.dlc_name;
}

void SodaInstallerImplChromeOS::UninstallSoda(PrefService* global_prefs) {
  soda_binary_installed_ = false;
  SetSodaBinaryPath(base::FilePath());
  ash::DlcserviceClient::Get()->Uninstall(
      kSodaDlcName, base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                                   base::Unretained(this), kSodaDlcName));
  // We iterate through all languages and check for installation, in order to
  // decide what's happened.
  for (const auto& it : available_languages_) {
    const auto path = GetLanguagePath(it.first);
    if (!path.empty()) {
      const auto dlc_name = it.second.dlc_name;
      LOG(ERROR) << "Removing dlc " << dlc_name << " for " << it.first;
      ash::DlcserviceClient::Get()->Uninstall(
          dlc_name, base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                                   base::Unretained(this), dlc_name));
    }
  }
  installed_languages_.clear();
  language_pack_progress_.clear();
  SodaInstaller::UnregisterLanguages(global_prefs);
  installed_language_paths_.clear();
  global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time());
}

void SodaInstallerImplChromeOS::SetSodaBinaryPath(base::FilePath new_path) {
  soda_lib_path_ = new_path;
}

void SodaInstallerImplChromeOS::SetLanguagePath(
    const LanguageCode language_code,
    base::FilePath new_path) {
  installed_language_paths_[language_code] = new_path;
}

void SodaInstallerImplChromeOS::OnSodaInstalled(
    const base::Time start_time,
    const ash::DlcserviceClient::InstallResult& install_result) {
  is_soda_downloading_ = false;
  if (install_result.error == dlcservice::kErrorNone) {
    soda_binary_installed_ = true;
    soda_progress_ = 1.0;
    SetSodaBinaryPath(base::FilePath(install_result.root_path));
    for (const auto& available_lang : available_languages_) {
      // Check every installed language and notify on it, in case the language
      // came before soda.
      if (IsLanguageInstalled(available_lang.second.language_code)) {
        NotifyOnSodaInstalled(available_lang.second.language_code);
      }
    }

    base::UmaHistogramTimes(kSodaBinaryInstallationSuccessTimeTaken,
                            base::Time::Now() - start_time);
  } else {
    soda_binary_installed_ = false;
    soda_progress_ = 0.0;
    NotifyOnSodaInstallError(LanguageCode::kNone,
                             DlcCodeToSodaErrorCode(install_result.error));
    base::UmaHistogramTimes(kSodaBinaryInstallationFailureTimeTaken,
                            base::Time::Now() - start_time);
  }

  base::UmaHistogramBoolean(kSodaBinaryInstallationResult,
                            install_result.error == dlcservice::kErrorNone);
}

void SodaInstallerImplChromeOS::OnLanguageInstalled(
    const LanguageCode language_code,
    const std::string language,
    const base::Time start_time,
    const ash::DlcserviceClient::InstallResult& install_result) {
  language_pack_progress_.erase(language_code);
  if (install_result.error == dlcservice::kErrorNone) {
    installed_languages_.insert(language_code);
    SetLanguagePath(language_code, base::FilePath(install_result.root_path));
    if (soda_binary_installed_) {
      NotifyOnSodaInstalled(language_code);
    }
    base::UmaHistogramTimes(
        GetInstallationSuccessTimeMetricForLanguage(language),
        base::Time::Now() - start_time);

  } else {
    // TODO: Notify the observer of the specific language pack that failed
    // to install. ChromeOS currently only supports the en-US language pack.
    NotifyOnSodaInstallError(language_code,
                             DlcCodeToSodaErrorCode(install_result.error));

    base::UmaHistogramTimes(
        GetInstallationFailureTimeMetricForLanguage(language),
        base::Time::Now() - start_time);
  }

  base::UmaHistogramBoolean(GetInstallationResultMetricForLanguage(language),
                            install_result.error == dlcservice::kErrorNone);
}

void SodaInstallerImplChromeOS::OnSodaProgress(double progress) {
  soda_progress_ = progress;
  OnSodaCombinedProgress();
}

void SodaInstallerImplChromeOS::OnLanguageProgress(
    const LanguageCode language_code,
    double progress) {
  language_pack_progress_[language_code] = progress;
  OnSodaCombinedProgress();
}

void SodaInstallerImplChromeOS::OnSodaCombinedProgress() {
  // Each language progress is weighed a little with the overall soda progress,
  // so that we only reach 100% if and only if soda binary itself is installed.
  // When the binary is installed, we use the plain percentage.
  const double language_weight = 4.0;
  for (const auto& per_language_progress : language_pack_progress_) {
    double progress = per_language_progress.second;

    // If SODA is downloading, report a combined progress for this language.
    if (is_soda_downloading_) {
      progress =
          (soda_progress_ + language_weight * progress) / (1 + language_weight);
    }
    NotifyOnSodaProgress(per_language_progress.first,
                         base::ClampFloor(100 * progress));
  }
}

void SodaInstallerImplChromeOS::OnDlcUninstalled(std::string_view dlc_id,
                                                 std::string_view err) {
  if (err != dlcservice::kErrorNone) {
    LOG(ERROR) << "Failed to uninstall DLC " << dlc_id << ". Error: " << err;
  } else {
    LOG(ERROR) << "Successful uninstall of dlc " << dlc_id;
  }
}

}  // namespace speech
