// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer_impl_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/pref_names.h"
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

SodaInstallerImplChromeOS::SodaInstallerImplChromeOS() = default;

SodaInstallerImplChromeOS::~SodaInstallerImplChromeOS() = default;

base::FilePath SodaInstallerImplChromeOS::GetSodaBinaryPath() const {
  return soda_lib_path_;
}

base::FilePath SodaInstallerImplChromeOS::GetLanguagePath(
    const std::string& language) const {
  // TODO(crbug.com/1161569): Support multiple languages. Currently only en-US
  // is supported.
  DCHECK_EQ(language, kUsEnglishLocale);
  return language_path_;
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
  SetLanguagePath(base::FilePath());

  // TODO(crbug.com/1055150): Compare user's language to a list of
  // supported languages and map it to a DLC ID. For now, default to
  // installing the SODA English-US DLC.
  DCHECK_EQ(language, kUsEnglishLocale);

  language_pack_progress_.insert({LanguageCode::kEnUs, 0.0});

  dlcservice::InstallRequest install_request;
  install_request.set_id(kSodaEnglishUsDlcName);
  ash::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&SodaInstallerImplChromeOS::OnLanguageInstalled,
                     base::Unretained(this), LanguageCode::kEnUs,
                     base::Time::Now()),
      base::BindRepeating(&SodaInstallerImplChromeOS::OnLanguageProgress,
                          base::Unretained(this)));
}

std::vector<std::string> SodaInstallerImplChromeOS::GetAvailableLanguages()
    const {
  // TODO(crbug.com/1161569): SODA is only available for English right now.
  // Update this to check available languages.
  return {kUsEnglishLocale};
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
  SetLanguagePath(base::FilePath());
  ash::DlcserviceClient::Get()->Uninstall(
      kSodaEnglishUsDlcName,
      base::BindOnce(&SodaInstallerImplChromeOS::OnDlcUninstalled,
                     base::Unretained(this), kSodaEnglishUsDlcName));
  global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time());
}

void SodaInstallerImplChromeOS::SetSodaBinaryPath(base::FilePath new_path) {
  soda_lib_path_ = new_path;
}

void SodaInstallerImplChromeOS::SetLanguagePath(base::FilePath new_path) {
  language_path_ = new_path;
}

void SodaInstallerImplChromeOS::OnSodaInstalled(
    const base::Time start_time,
    const ash::DlcserviceClient::InstallResult& install_result) {
  is_soda_downloading_ = false;
  if (install_result.error == dlcservice::kErrorNone) {
    soda_binary_installed_ = true;
    SetSodaBinaryPath(base::FilePath(install_result.root_path));
    // TODO(crbug.com/1161569): SODA is only available for English right now.
    // Update this to notify on all installed languages.
    if (IsLanguageInstalled(LanguageCode::kEnUs))
      NotifyOnSodaInstalled(LanguageCode::kEnUs);

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
    SetLanguagePath(base::FilePath(install_result.root_path));
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

void SodaInstallerImplChromeOS::OnLanguageProgress(double progress) {
  language_pack_progress_[LanguageCode::kEnUs] = progress;
  OnSodaCombinedProgress();
}

void SodaInstallerImplChromeOS::OnSodaCombinedProgress() {
  // TODO(crbug.com/1055150): Consider updating this implementation.
  // e.g.: (1) starting progress from 0% if we are downloading language
  // only (2) weighting download progress proportionally to DLC binary size.
  double language_progress = 0.0;
  if (base::Contains(language_pack_progress_, LanguageCode::kEnUs))
    language_progress = language_pack_progress_[LanguageCode::kEnUs];

  const double progress = (soda_progress_ + language_progress) / 2;
  // TODO: Notify the observer of the specific language pack that is currently
  // being installed. ChromeOS currently only supports the en-US language pack.
  NotifyOnSodaProgress(LanguageCode::kEnUs, base::ClampFloor(100 * progress));
}

void SodaInstallerImplChromeOS::OnDlcUninstalled(const std::string& dlc_id,
                                                 const std::string& err) {
  if (err != dlcservice::kErrorNone) {
    LOG(ERROR) << "Failed to uninstall DLC " << dlc_id << ". Error: " << err;
  }
}

}  // namespace speech
