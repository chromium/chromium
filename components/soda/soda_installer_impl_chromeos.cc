// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer_impl_chromeos.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
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

}  // namespace

SodaInstallerImplChromeOS::SodaInstallerImplChromeOS() {
  available_languages_ = ConstructAvailableLanguages();
}

base::flat_map<std::string, SodaInstallerImplChromeOS::LanguageInfo>
SodaInstallerImplChromeOS::ConstructAvailableLanguages() const {
  base::flat_map<std::string, LanguageInfo> available_languages;
  // Defaults checked in.
  available_languages.insert(
      {kUsEnglishLocale, {kSodaEnglishUsDlcName, LanguageCode::kEnUs}});
  if (!base::FeatureList::IsEnabled(kCrosExpandSodaLanguages)) {
    return available_languages;
  }

  available_languages.insert({"ja-JP", {"", LanguageCode::kJaJp}});
  available_languages.insert({"de-DE", {"", LanguageCode::kDeDe}});
  available_languages.insert({"es-ES", {"", LanguageCode::kEsEs}});
  available_languages.insert({"fr-FR", {"", LanguageCode::kFrFr}});
  available_languages.insert({"it-IT", {"", LanguageCode::kItIt}});
  available_languages.insert({"en-CA", {"", LanguageCode::kEnCa}});
  available_languages.insert({"en-AU", {"", LanguageCode::kEnAu}});
  available_languages.insert({"en-GB", {"", LanguageCode::kEnGb}});
  available_languages.insert({"en-IE", {"", LanguageCode::kEnIe}});
  available_languages.insert({"en-SG", {"", LanguageCode::kEnSg}});
  available_languages.insert({"fr-BE", {"", LanguageCode::kFrBe}});
  available_languages.insert({"fr-FR", {"", LanguageCode::kFrFr}});
  available_languages.insert({"fr-CH", {"", LanguageCode::kFrCh}});
  available_languages.insert({"en-IN", {"", LanguageCode::kEnIn}});
  available_languages.insert({"it-CH", {"", LanguageCode::kItCh}});
  available_languages.insert({"de-AT", {"", LanguageCode::kDeAt}});
  available_languages.insert({"de-BE", {"", LanguageCode::kDeBe}});
  available_languages.insert({"de-CH", {"", LanguageCode::kDeCh}});
  available_languages.insert({"es-US", {"", LanguageCode::kEsUs}});
  available_languages.insert({"da-DK", {"", LanguageCode::kDaDk}});
  available_languages.insert({"fr-CA", {"", LanguageCode::kFrCa}});
  available_languages.insert({"hi-IN", {"", LanguageCode::kHiIn}});
  available_languages.insert({"id-ID", {"", LanguageCode::kIdId}});
  available_languages.insert({"ko-KR", {"", LanguageCode::kKoKr}});
  available_languages.insert({"id-ID", {"", LanguageCode::kIdId}});
  available_languages.insert({"nb-NO", {"", LanguageCode::kNbNo}});
  available_languages.insert({"nl-NL", {"", LanguageCode::kNlNl}});
  available_languages.insert({"pl-PL", {"", LanguageCode::kPlPl}});
  available_languages.insert({"sv-SE", {"", LanguageCode::kSvSe}});
  available_languages.insert({"th-TH", {"", LanguageCode::kThTh}});
  available_languages.insert({"tr-TR", {"", LanguageCode::kTrTr}});
  available_languages.insert({"zh-TW", {"", LanguageCode::kZhTw}});
  available_languages.insert({"zh-CN", {"", LanguageCode::kZhCn}});

  // Add in from feature flags. the value is of the format:
  // "en-AU:libsoda-modelname,de-CH:libsoda-pizzaface"
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
    if (lang_model_pair[1].rfind("libsoda", 0) == std::string::npos) {
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
    LOG(DFATAL) << "Asked for path to not installed language " << language;
    return base::FilePath();
  }
  return it->second;
}

void SodaInstallerImplChromeOS::InstallSoda(PrefService* global_prefs) {
  if (never_download_soda_for_testing_)
    return;
  // Clear cached path in case this is a reinstallation (path could
  // change).
  SetSodaBinaryPath(base::FilePath());

  soda_binary_installed_ = false;
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
  // TODO(crbug.com/1161569): SODA is only available for en-US right now.
  DCHECK_EQ(language, kUsEnglishLocale);
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
                     language_info->second.language_code, base::Time::Now()),
      base::BindRepeating(&SodaInstallerImplChromeOS::OnLanguageProgress,
                          base::Unretained(this),
                          language_info->second.language_code));
}

void SodaInstallerImplChromeOS::UninstallLanguage(const std::string& language,
                                                  PrefService* global_prefs) {
  // TODO(crbug.com/1161569): SODA is only available for en-US right now.
  // Update this to uninstall the language pack.
}

std::vector<std::string> SodaInstallerImplChromeOS::GetAvailableLanguages()
    const {
  std::vector<std::string> languages;
  for (const auto& it : available_languages_) {
    languages.push_back(it.first);
  }
  return languages;
}

void SodaInstallerImplChromeOS::UninstallSoda(PrefService* global_prefs) {
  soda_binary_installed_ = false;
  SetSodaBinaryPath(base::FilePath());
  ash::DlcserviceClient::Get()->Uninstall(
      kSodaDlcName, base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                                   base::Unretained(this), kSodaDlcName));
  installed_languages_.clear();
  language_pack_progress_.clear();
  SodaInstaller::UnregisterLanguages(global_prefs);
  installed_language_paths_.clear();
  ash::DlcserviceClient::Get()->Uninstall(
      kSodaEnglishUsDlcName,
      base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                     base::Unretained(this), kSodaEnglishUsDlcName));
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
        GetInstallationSuccessTimeMetricForLanguagePack(language_code),
        base::Time::Now() - start_time);

  } else {
    // TODO: Notify the observer of the specific language pack that failed
    // to install. ChromeOS currently only supports the en-US language pack.
    NotifyOnSodaInstallError(language_code,
                             DlcCodeToSodaErrorCode(install_result.error));

    base::UmaHistogramTimes(
        GetInstallationFailureTimeMetricForLanguagePack(language_code),
        base::Time::Now() - start_time);
  }

  base::UmaHistogramBoolean(
      GetInstallationResultMetricForLanguagePack(language_code),
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
  // TODO(crbug.com/1055150): Consider updating this implementation.
  // e.g.: (1) starting progress from 0% if we are downloading language
  // only (2) weighting download progress proportionally to DLC binary size.
  double language_progress = 0.0;
  for (const auto& per_language_progress : language_pack_progress_) {
    language_progress += per_language_progress.second;
  }

  const double progress = (soda_progress_ + language_progress) /
                          (1 + language_pack_progress_.size());
  // Notify all progressing languages that there's been some movement.
  for (const auto& per_language_progress : language_pack_progress_) {
    NotifyOnSodaProgress(per_language_progress.first,
                         base::ClampFloor(100 * progress));
  }
}

void SodaInstallerImplChromeOS::OnDlcUninstalled(const std::string& dlc_id,
                                                 const std::string& err) {
  if (err != dlcservice::kErrorNone) {
    LOG(ERROR) << "Failed to uninstall DLC " << dlc_id << ". Error: " << err;
  }
}

}  // namespace speech
