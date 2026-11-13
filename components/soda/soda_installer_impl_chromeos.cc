// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer_impl_chromeos.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/timer/timer.h"
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

constexpr float kInitialSodaRetryTimeSeconds = 1.0;
constexpr float kMaxSodaRetryTimeSeconds = 32.0;

SodaInstaller::ErrorCode DlcCodeToSodaErrorCode(std::string_view code) {
  return (code == dlcservice::kErrorNeedReboot)
             ? SodaInstaller::ErrorCode::kNeedsReboot
             : SodaInstaller::ErrorCode::kUnspecifiedError;
}
const constexpr char* const kDefaultCrOSEnabledLanguages[] = {"da-DK", "nl-NL",
                                                              "nb-NO", "sv-SE"};

struct LanguageInfo {
  std::string_view dlc_name;
  LanguageCode language_code;
};

// Defaults checked in.
constexpr auto kDefaultAvailableLanguages =
    base::MakeFixedFlatMap<std::string_view, LanguageInfo>({
        {"cmn-Hans-CN", {"libsoda-model-zh-cn-df24d2", LanguageCode::kZhCn}},
        {"cmn-Hant-TW", {"libsoda-model-zh-tw-df24d2", LanguageCode::kZhTw}},
        {"de-AT", {"libsoda-model-de-at-df24d2", LanguageCode::kDeAt}},
        {"de-BE", {"libsoda-model-de-be-df24d2", LanguageCode::kDeBe}},
        {"de-CH", {"libsoda-model-de-ch-df24d2", LanguageCode::kDeCh}},
        {"de-DE", {"libsoda-model-de-de-df24d2", LanguageCode::kDeDe}},
        {"en-AU", {"libsoda-model-en-au-df24d2", LanguageCode::kEnAu}},
        {"en-CA", {"libsoda-model-en-ca-df24d2", LanguageCode::kEnCa}},
        {"en-GB", {"libsoda-model-en-gb-df24d2", LanguageCode::kEnGb}},
        {"en-IE", {"libsoda-model-en-ie-df24d2", LanguageCode::kEnIe}},
        {"en-IN", {"libsoda-model-en-in-df24d2", LanguageCode::kEnIn}},
        {"en-SG", {"libsoda-model-en-sg-df24d2", LanguageCode::kEnSg}},
        {"en-US", {"libsoda-model-en-us-df24d2", LanguageCode::kEnUs}},
        {"es-ES", {"libsoda-model-es-us-df24d2", LanguageCode::kEsEs}},
        {"es-US", {"libsoda-model-es-us-df24d2", LanguageCode::kEsUs}},
        {"fr-BE", {"libsoda-model-fr-be-df24d2", LanguageCode::kFrBe}},
        {"fr-CA", {"libsoda-model-fr-ca-df24d2", LanguageCode::kFrCa}},
        {"fr-CH", {"libsoda-model-fr-ch-df24d2", LanguageCode::kFrCh}},
        {"fr-FR", {"libsoda-model-fr-fr-df24d2", LanguageCode::kFrFr}},
        {"hi-IN", {"libsoda-model-hi-in-df24d2", LanguageCode::kHiIn}},
        {"id-ID", {"libsoda-model-id-id-df24d2", LanguageCode::kIdId}},
        {"it-CH", {"libsoda-model-it-ch-df24d2", LanguageCode::kItCh}},
        {"it-IT", {"libsoda-model-it-it-df24d2", LanguageCode::kItIt}},
        {"ja-JP", {"libsoda-model-ja-jp-df24d2", LanguageCode::kJaJp}},
        {"ko-KR", {"libsoda-model-ko-kr-df24d2", LanguageCode::kKoKr}},
        {"pl-PL", {"libsoda-model-pl-pl-df24d2", LanguageCode::kPlPl}},
        {"pt-BR", {"libsoda-model-pt-br-df24d2", LanguageCode::kPtBr}},
        {"ru-RU", {"libsoda-model-ru-ru-df24d2", LanguageCode::kRuRu}},
        {"th-TH", {"libsoda-model-th-th-df24d2", LanguageCode::kThTh}},
        {"tr-TR", {"libsoda-model-tr-tr-df24d2", LanguageCode::kTrTr}},
        {"vi-VN", {"libsoda-model-vi-vn-df24d2", LanguageCode::kViVn}},
    });

constexpr auto kConchAvailableLanguages =
    base::MakeFixedFlatMap<std::string_view, LanguageInfo>({
        {"da-DK", {"libsoda-model-da-dk-cnch24d3", LanguageCode::kDaDk}},
        {"de-AT", {"libsoda-model-de-at-cnch24d3", LanguageCode::kDeAt}},
        {"de-BE", {"libsoda-model-de-be-cnch24d3", LanguageCode::kDeBe}},
        {"de-CH", {"libsoda-model-de-ch-cnch24d3", LanguageCode::kDeCh}},
        {"de-DE", {"libsoda-model-de-de-cnch24d3", LanguageCode::kDeDe}},
        {"en-AU", {"libsoda-model-en-au-cnch24d3", LanguageCode::kEnAu}},
        {"en-CA", {"libsoda-model-en-ca-cnch24d3", LanguageCode::kEnCa}},
        {"en-GB", {"libsoda-model-en-gb-cnch24d3", LanguageCode::kEnGb}},
        {"en-IE", {"libsoda-model-en-ie-cnch24d3", LanguageCode::kEnIe}},
        {"en-IN", {"libsoda-model-en-in-cnch24d3", LanguageCode::kEnIn}},
        {"en-SG", {"libsoda-model-en-sg-cnch24d3", LanguageCode::kEnSg}},
        {"en-US", {"libsoda-model-en-us-cnch24d3", LanguageCode::kEnUs}},
        {"es-ES", {"libsoda-model-es-es-cnch24d3", LanguageCode::kEsEs}},
        {"es-US", {"libsoda-model-es-us-cnch24d3", LanguageCode::kEsUs}},
        {"fr-BE", {"libsoda-model-fr-be-cnch24d3", LanguageCode::kFrBe}},
        {"fr-CA", {"libsoda-model-fr-ca-cnch24d3", LanguageCode::kFrCa}},
        {"fr-CH", {"libsoda-model-fr-Ch-cnch24d3", LanguageCode::kFrCh}},
        {"fr-FR", {"libsoda-model-fr-fr-cnch24d3", LanguageCode::kFrFr}},
        {"hi-IN", {"libsoda-model-hi-in-cnch24d3", LanguageCode::kHiIn}},
        {"it-IT", {"libsoda-model-it-it-cnch24d3", LanguageCode::kItIt}},
        {"ja-JP", {"libsoda-model-ja-jp-cnch24d3", LanguageCode::kJaJp}},
        {"ko-KR", {"libsoda-model-ko-kr-cnch24d3", LanguageCode::kKoKr}},
        {"nb-NO", {"libsoda-model-nb-no-cnch24d3", LanguageCode::kNbNo}},
        {"nl-NL", {"libsoda-model-nl-nl-cnch24d3", LanguageCode::kNlNl}},
        {"sv-SE", {"libsoda-model-sv-se-cnch24d3", LanguageCode::kSvSe}},
    });

bool IsConchLanguagesEnabled() {
  return base::FeatureList::IsEnabled(
             kFeatureManagementCrosSodaConchLanguages) &&
         base::FeatureList::IsEnabled(kCrosSodaConchLanguages);
}

const LanguageInfo* FindAvailableLanguage(std::string_view language) {
  const std::string normalized_language = MaybeMapToChineseLocale(language);
  if (IsConchLanguagesEnabled() &&
      !(normalized_language == kUsEnglishLocale &&
        base::FeatureList::IsEnabled(
            kAlwaysUsePreloadedSodaModelEnUsForTesting))) {
    if (const LanguageInfo* info =
            base::FindOrNull(kConchAvailableLanguages, normalized_language)) {
      return info;
    }
  }
  return base::FindOrNull(kDefaultAvailableLanguages, normalized_language);
}

}  // namespace

SodaInstallerImplChromeOS::SodaInstallerImplChromeOS() {
  soda_backoff_seconds_ = kInitialSodaRetryTimeSeconds;
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

SodaInstallerImplChromeOS::~SodaInstallerImplChromeOS() = default;

base::FilePath SodaInstallerImplChromeOS::GetSodaBinaryPath() const {
  return soda_lib_path_;
}

base::FilePath SodaInstallerImplChromeOS::GetLanguagePath(
    std::string_view language) const {
  const LanguageInfo* language_info = FindAvailableLanguage(language);
  if (!language_info) {
    LOG(DFATAL) << "Asked for unavailable language " << language;
    return base::FilePath();
  }
  auto it = installed_language_paths_.find(language_info->language_code);
  if (it == installed_language_paths_.end()) {
    return base::FilePath();
  }
  return it->second;
}

void SodaInstallerImplChromeOS::OnSodaInstallRetry() {
  // we just run a soda install here. Turns out that we don't actually need the
  // global prefs for this (at time of writing).  Internally runs the install
  // process again, and then if that fails, continue retrying until
  // exhausted. Timeout for retry doubles on every try and retries happen for
  // all non-OK errors, although some errors are permanent.
  InstallSoda(nullptr);
}

void SodaInstallerImplChromeOS::OnSodaLanguageInstallRetry() {
  // We move the retry langs to a temporary here so that, just in case failure
  // is instant and not out of line, we dont clobber it later.
  base::flat_set<std::string> retry_languages(retry_languages_to_install_);
  retry_languages_to_install_.clear();

  for (const auto& language : retry_languages) {
    InstallLanguageInternal(language);
  }
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

void SodaInstallerImplChromeOS::InstallLanguage(std::string_view language,
                                                PrefService* global_prefs) {
  if (never_download_soda_for_testing_) {
    return;
  }
  SodaInstaller::RegisterLanguage(language, global_prefs);
  // Clear cached path in case this is a reinstallation (path could
  // change).
  InstallLanguageInternal(language);
}
void SodaInstallerImplChromeOS::InstallLanguageInternal(
    std::string_view language) {
  const LanguageInfo* language_info = FindAvailableLanguage(language);
  if (!language_info) {
    LOG(DFATAL)
        << "Language " << language
        << " not in list of available languages. refusing to install anything.";
    return;
  }

  SetLanguagePath(language_info->language_code, base::FilePath());
  language_pack_progress_.insert({language_info->language_code, 0.0});

  dlcservice::InstallRequest install_request;
  install_request.set_id(language_info->dlc_name);
  ash::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&SodaInstallerImplChromeOS::OnLanguageInstalled,
                     base::Unretained(this), language_info->language_code,
                     std::string(language), base::Time::Now()),
      base::BindRepeating(&SodaInstallerImplChromeOS::OnLanguageProgress,
                          base::Unretained(this),
                          language_info->language_code));
}

void SodaInstallerImplChromeOS::UninstallLanguage(std::string_view language,
                                                  PrefService* global_prefs) {
  SodaInstaller::UnregisterLanguage(language, global_prefs);
  const LanguageInfo* language_info = FindAvailableLanguage(language);
  // Remove from retry list in case it's in there.
  retry_languages_to_install_.erase(language);
  if (!language_info) {
    LOG(DFATAL) << "Unable to uninstall language " << language
                << " as it is not in the list of available languages.";
    return;
  }
  const std::string dlc_name(language_info->dlc_name);
  installed_languages_.erase(language_info->language_code);
  installed_language_paths_.erase(language_info->language_code);
  language_pack_progress_.erase(language_info->language_code);

  ash::DlcserviceClient::Get()->Uninstall(
      dlc_name, base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                               base::Unretained(this), dlc_name));
}

std::vector<std::string> SodaInstallerImplChromeOS::GetAvailableLanguages()
    const {
  std::vector<std::string> languages;
  for (const auto& it : kDefaultAvailableLanguages) {
    languages.emplace_back(it.first);
  }
  if (IsConchLanguagesEnabled()) {
    for (const auto& it : kConchAvailableLanguages) {
      if (!kDefaultAvailableLanguages.contains(it.first)) {
        languages.emplace_back(it.first);
      }
    }
  }
  return languages;
}

std::vector<std::string>
SodaInstallerImplChromeOS::GetLiveCaptionEnabledLanguages() const {
  std::vector<std::string> enabled_languages =
      SodaInstaller::GetLiveCaptionEnabledLanguages();
  // extra CrOS languages.
  if (IsConchLanguagesEnabled()) {
    enabled_languages.reserve(enabled_languages.size() +
                              std::size(kDefaultCrOSEnabledLanguages));
    for (const char* const enabled_language : kDefaultCrOSEnabledLanguages) {
      enabled_languages.push_back(enabled_language);
    }
  }
  return enabled_languages;
}

std::string SodaInstallerImplChromeOS::GetLanguageDlcNameForLocale(
    std::string_view locale) const {
  const LanguageInfo* language_info = FindAvailableLanguage(locale);
  if (!language_info) {
    LOG(DFATAL) << "Asked for unavailable language " << locale;
    return std::string();
  }

  return std::string(language_info->dlc_name);
}

void SodaInstallerImplChromeOS::UninstallSoda(PrefService* global_prefs) {
  soda_binary_installed_ = false;
  soda_install_retry_timer_.Stop();
  soda_backoff_seconds_ = kInitialSodaRetryTimeSeconds;
  SetSodaBinaryPath(base::FilePath());
  ash::DlcserviceClient::Get()->Uninstall(
      kSodaDlcName, base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                                   base::Unretained(this), kSodaDlcName));
  // We iterate through all languages and check for installation, in order to
  // decide what's happened.
  for (const std::string& language : GetAvailableLanguages()) {
    const base::FilePath path = GetLanguagePath(language);
    if (!path.empty()) {
      const std::string dlc_name = GetLanguageDlcNameForLocale(language);
      LOG(ERROR) << "Removing dlc " << dlc_name << " for " << language;
      ash::DlcserviceClient::Get()->Uninstall(
          dlc_name, base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                                   base::Unretained(this), dlc_name));
    }
  }
  installed_languages_.clear();
  language_pack_progress_.clear();
  SodaInstaller::UnregisterLanguages(global_prefs);
  installed_language_paths_.clear();
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
    soda_backoff_seconds_ = kInitialSodaRetryTimeSeconds;
    soda_install_retry_timer_.Stop();
    SetSodaBinaryPath(base::FilePath(install_result.root_path));
    for (const std::string& available_lang : GetAvailableLanguages()) {
      // Check every installed language and notify on it, in case the language
      // came before soda.
      const LanguageCode language_code =
          FindAvailableLanguage(available_lang)->language_code;
      if (IsLanguageInstalled(language_code)) {
        NotifyOnSodaInstalled(language_code);
      }
    }

    base::UmaHistogramTimes(kSodaBinaryInstallationSuccessTimeTaken,
                            base::Time::Now() - start_time);
  } else if (soda_backoff_seconds_ <= kMaxSodaRetryTimeSeconds) {
    // TODO(robsc): Pipe the original start time through here, so that total
    // time to fail or install is tracked correctly.
    soda_install_retry_timer_.Start(
        FROM_HERE, base::Seconds(soda_backoff_seconds_), this,
        &SodaInstallerImplChromeOS::OnSodaInstallRetry);
    soda_backoff_seconds_ *= 2;
  } else {
    soda_binary_installed_ = false;
    soda_progress_ = 0.0;
    soda_backoff_seconds_ = kInitialSodaRetryTimeSeconds;
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
    if (retry_languages_to_install_.empty()) {
      soda_languagepack_backoff_seconds_ = 1.0;
      languagepack_install_retry_timer_.Stop();
    }
    installed_languages_.insert(language_code);
    SetLanguagePath(language_code, base::FilePath(install_result.root_path));
    if (soda_binary_installed_) {
      NotifyOnSodaInstalled(language_code);
    }
    base::UmaHistogramTimes(
        GetInstallationSuccessTimeMetricForLanguage(language),
        base::Time::Now() - start_time);

  } else if (soda_languagepack_backoff_seconds_ <= kMaxSodaRetryTimeSeconds) {
    retry_languages_to_install_.insert(language);
    if (retry_languages_to_install_.size() == 1) {
      // let's set the timer now.
      languagepack_install_retry_timer_.Start(
          FROM_HERE, base::Seconds(soda_languagepack_backoff_seconds_), this,
          &SodaInstallerImplChromeOS::OnSodaLanguageInstallRetry);
      soda_languagepack_backoff_seconds_ *= 2;
    }
  } else {
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
