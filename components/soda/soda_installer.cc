// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer.h"

#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/soda/constants.h"
#include "components/soda/pref_names.h"
#include "media/base/media_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

constexpr int kSodaCleanUpDelayInDays = 30;
const constexpr char* const kDefaultEnabledLanguages[] = {
    "en-US", "fr-FR", "it-IT", "de-DE",       "es-ES",
    "ja-JP", "hi-IN", "pt-BR", "ko-KR",       "pl-PL",
    "th-TH", "tr-TR", "id-ID", "cmn-Hans-CN", "cmn-Hant-TW"};

}  // namespace

namespace speech {

SodaInstaller* g_instance = nullptr;

// static
SodaInstaller* SodaInstaller::GetInstance() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(
      base::FeatureList::IsEnabled(ash::features::kOnDeviceSpeechRecognition));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return g_instance;
}

SodaInstaller::SodaInstaller() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

SodaInstaller::~SodaInstaller() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void SodaInstaller::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kSodaScheduledDeletionTime, base::Time());
  SodaInstaller::RegisterRegisteredLanguagePackPref(registry);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Handle non-Chrome-OS logic here. We need to keep the implementation of this
  // method in the parent class to avoid duplicate declaration build errors
  // (specifically on Windows).
  registry->RegisterFilePathPref(prefs::kSodaBinaryPath, base::FilePath());

  // Register language pack config path preferences.
  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    registry->RegisterFilePathPref(config.config_path_pref, base::FilePath());
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

void SodaInstaller::Init(PrefService* profile_prefs,
                         PrefService* global_prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition) ||
      soda_installer_initialized_) {
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  if (soda_installer_initialized_) {
#endif
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsAnyFeatureUsingSodaEnabled(profile_prefs) ||
      base::FeatureList::IsEnabled(media::kOnDeviceWebSpeech)) {
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsAnyFeatureUsingSodaEnabled(profile_prefs)) {
#endif
    soda_installer_initialized_ = true;
    // Set the SODA uninstaller time to NULL time so that it doesn't get
    // uninstalled when features are using it.
    global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time());
    SodaInstaller::GetInstance()->InstallSoda(global_prefs);
    InitLanguages(profile_prefs, global_prefs);
  } else {
    base::Time deletion_time =
        global_prefs->GetTime(prefs::kSodaScheduledDeletionTime);
    if (!deletion_time.is_null() && deletion_time <= base::Time::Now()) {
      UninstallSoda(global_prefs);
      soda_installer_initialized_ = false;
    }
  }
}

void SodaInstaller::InitLanguages(PrefService* profile_prefs,
                                  PrefService* global_prefs) {
  if (global_prefs->GetList(prefs::kSodaRegisteredLanguagePacks).empty()) {
    RegisterLanguage(prefs::GetLiveCaptionLanguageCode(profile_prefs),
                     global_prefs);
  }

  for (const auto& language :
       global_prefs->GetList(prefs::kSodaRegisteredLanguagePacks)) {
    SodaInstaller::GetInstance()->InstallLanguage(language.GetString(),
                                                  global_prefs);
  }
}

void SodaInstaller::SetUninstallTimer(PrefService* profile_prefs,
                                      PrefService* global_prefs) {
  // Do not schedule uninstallation if any SODA client features are still
  // enabled.
  if (IsAnyFeatureUsingSodaEnabled(profile_prefs))
    return;

  // Schedule deletion.
  global_prefs->SetTime(
      prefs::kSodaScheduledDeletionTime,
      base::Time::Now() + base::Days(kSodaCleanUpDelayInDays));
}

std::string SodaInstaller::GetLanguageDlcNameForLocale(
    const std::string& locale) const {
  return std::string();
}

bool SodaInstaller::IsSodaInstalled(LanguageCode language_code) const {
  return (soda_binary_installed_ && IsLanguageInstalled(language_code));
}

bool SodaInstaller::IsSodaBinaryInstalled() const {
  return soda_binary_installed_;
}

const std::set<LanguageCode> SodaInstaller::InstalledLanguages() const {
  return installed_languages_;
}

bool SodaInstaller::IsLanguageInstalled(LanguageCode language_code) const {
  return base::Contains(installed_languages_, language_code);
}

void SodaInstaller::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SodaInstaller::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SodaInstaller::NotifySodaInstalledForTesting(LanguageCode language_code) {
  // TODO: Call the actual functions in SodaInstallerImpl and
  // SodaInstallerImpleChromeOS that do this logic
  // (e.g. SodaInstallerImpl::OnSodaBinaryInstalled) rather than faking it.

  // If language code is none, this signifies that the SODA binary installed.
  if (language_code == LanguageCode::kNone) {
    soda_binary_installed_ = true;
    is_soda_downloading_ = false;
    for (LanguageCode installed_language : installed_languages_) {
      NotifyOnSodaInstalled(installed_language);
    }
    return;
  }

  // Otherwise, this means a language pack installed.
  installed_languages_.insert(language_code);
  if (base::Contains(language_pack_progress_, language_code))
    language_pack_progress_.erase(language_code);
  if (soda_binary_installed_)
    NotifyOnSodaInstalled(language_code);
}

void SodaInstaller::NotifySodaErrorForTesting(LanguageCode language_code,
                                              ErrorCode error_code) {
  // TODO: Call the actual functions in SodaInstallerImpl and
  // SodaInstallerImpleChromeOS that do this logic rather than faking it.
  if (language_code == LanguageCode::kNone) {
    // Error with the SODA binary download.
    soda_binary_installed_ = false;
    is_soda_downloading_ = false;
    language_pack_progress_.clear();
  } else {
    // Error with the language pack download.
    if (base::Contains(language_pack_progress_, language_code))
      language_pack_progress_.erase(language_code);
  }
  NotifyOnSodaInstallError(language_code, error_code);
}

void SodaInstaller::UninstallSodaForTesting() {
  soda_binary_installed_ = false;
  is_soda_downloading_ = false;
  soda_installer_initialized_ = false;
  installed_languages_.clear();
  language_pack_progress_.clear();
}

void SodaInstaller::NotifySodaProgressForTesting(int progress,
                                                 LanguageCode language_code) {
  // TODO: Call the actual functions in SodaInstallerImpl and
  // SodaInstallerImpleChromeOS that do this logic rather than faking it.
  if (language_code == LanguageCode::kNone) {
    // SODA binary download progress.
    soda_binary_installed_ = false;
    is_soda_downloading_ = true;
  } else {
    // Language pack download progress.
    if (base::Contains(language_pack_progress_, language_code))
      language_pack_progress_.insert({language_code, progress});
    else
      language_pack_progress_[language_code] = progress;
  }
  NotifyOnSodaProgress(language_code, progress);
}

bool SodaInstaller::IsAnyLanguagePackInstalledForTesting() const {
  return !installed_languages_.empty();
}

void SodaInstaller::RegisterRegisteredLanguagePackPref(
    PrefRegistrySimple* registry) {
  // TODO: Default to one of the user's languages.
  base::Value::List default_languages;
  default_languages.Append(base::Value(kUsEnglishLocale));
  registry->RegisterListPref(prefs::kSodaRegisteredLanguagePacks,
                             std::move(default_languages));
}

void SodaInstaller::NotifyOnSodaInstalled(LanguageCode language_code) {
  error_codes_.erase(language_code);
  for (Observer& observer : observers_)
    observer.OnSodaInstalled(language_code);
}

void SodaInstaller::NotifyOnSodaInstallError(LanguageCode language_code,
                                             ErrorCode error_code) {
  error_codes_[language_code] = error_code;
  for (Observer& observer : observers_)
    observer.OnSodaInstallError(language_code, error_code);
}

void SodaInstaller::NotifyOnSodaProgress(LanguageCode language_code,
                                         int progress) {
  for (Observer& observer : observers_)
    observer.OnSodaProgress(language_code, progress);
}

void SodaInstaller::RegisterLanguage(const std::string& language,
                                     PrefService* global_prefs) {
  ScopedListPrefUpdate update(global_prefs,
                              prefs::kSodaRegisteredLanguagePacks);
  if (!base::Contains(*update, base::Value(language))) {
    update->Append(language);
  }
}

void SodaInstaller::UnregisterLanguage(const std::string& language,
                                       PrefService* global_prefs) {
  ScopedListPrefUpdate update(global_prefs,
                              prefs::kSodaRegisteredLanguagePacks);
  if (base::Contains(*update, base::Value(language))) {
    update->EraseValue(base::Value(language));
  }
}

void SodaInstaller::UnregisterLanguages(PrefService* global_prefs) {
  ScopedListPrefUpdate update(global_prefs,
                              prefs::kSodaRegisteredLanguagePacks);
  update->clear();
}

bool SodaInstaller::IsSodaDownloading(LanguageCode language_code) const {
  return is_soda_downloading_ ||
         base::Contains(language_pack_progress_, language_code);
}

std::optional<SodaInstaller::ErrorCode> SodaInstaller::GetSodaInstallErrorCode(
    LanguageCode language_code) const {
  if (IsSodaDownloading(language_code))
    return std::nullopt;

  const auto error_code = error_codes_.find(language_code);
  if (error_code != error_codes_.end())
    return error_code->second;
  return std::nullopt;
}

bool SodaInstaller::IsAnyFeatureUsingSodaEnabled(PrefService* prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return prefs->GetBoolean(prefs::kLiveCaptionEnabled) ||
         prefs->GetBoolean(ash::prefs::kAccessibilityDictationEnabled) ||
         prefs->GetBoolean(ash::prefs::kProjectorCreationFlowEnabled);
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  return prefs->GetBoolean(prefs::kLiveCaptionEnabled);
#endif
}

std::vector<std::string> SodaInstaller::GetLiveCaptionEnabledLanguages() const {
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
