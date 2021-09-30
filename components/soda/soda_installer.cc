// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/soda/constants.h"
#include "components/soda/pref_names.h"
#include "media/base/media_switches.h"

namespace {

constexpr int kSodaCleanUpDelayInDays = 30;

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
  for (const speech::SodaLanguagePackComponentConfig& config :
       speech::kLanguageComponentConfigs) {
    registry->RegisterFilePathPref(config.config_path_pref, base::FilePath());
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

void SodaInstaller::Init(PrefService* profile_prefs,
                         PrefService* global_prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition) ||
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) ||
#endif
      soda_installer_initialized_) {
    return;
  }

  if (IsAnyFeatureUsingSodaEnabled(profile_prefs)) {
    soda_installer_initialized_ = true;
    global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time::Now());
    SodaInstaller::GetInstance()->InstallSoda(global_prefs);

    if (global_prefs->GetList(prefs::kSodaRegisteredLanguagePacks)
            ->GetList()
            .empty()) {
      // TODO(crbug.com/1200667): Register the default language used by
      // Dictation on ChromeOS.
      // TODO(crbug.com/1165437): Register the default language used by
      // Projector on ChromeOS.
      RegisterLanguage(
          profile_prefs->GetString(prefs::kLiveCaptionLanguageCode),
          global_prefs);
    }

    for (const auto& language :
         global_prefs->GetList(prefs::kSodaRegisteredLanguagePacks)
             ->GetList()) {
      speech::SodaInstaller::GetInstance()->InstallLanguage(
          language.GetString(), global_prefs);
    }
  } else {
    base::Time deletion_time =
        global_prefs->GetTime(prefs::kSodaScheduledDeletionTime);
    if (!deletion_time.is_null() && deletion_time <= base::Time::Now()) {
      UninstallSoda(global_prefs);
      soda_installer_initialized_ = false;
    }
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
      base::Time::Now() + base::TimeDelta::FromDays(kSodaCleanUpDelayInDays));
}

bool SodaInstaller::IsSodaInstalled(speech::LanguageCode language_code) const {
  return (soda_binary_installed_ && IsLanguageInstalled(language_code));
}

bool SodaInstaller::IsAnyLanguagePackInstalled() const {
  return !installed_languages_.empty();
}

bool SodaInstaller::IsLanguageInstalled(
    speech::LanguageCode language_code) const {
  return installed_languages_.find(language_code) != installed_languages_.end();
}

void SodaInstaller::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SodaInstaller::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SodaInstaller::NotifySodaInstalledForTesting() {
  soda_binary_installed_ = true;
  is_soda_downloading_ = false;
  installed_languages_.insert(speech::LanguageCode::kEnUs);
  language_pack_progress_.clear();
  NotifyOnSodaInstalled();
}

void SodaInstaller::NotifySodaErrorForTesting() {
  soda_binary_installed_ = false;
  is_soda_downloading_ = false;
  installed_languages_.clear();
  language_pack_progress_.clear();
  NotifyOnSodaError();
}

void SodaInstaller::UninstallSodaForTesting() {
  soda_binary_installed_ = false;
  is_soda_downloading_ = false;
  soda_installer_initialized_ = false;
  installed_languages_.clear();
  language_pack_progress_.clear();
}

void SodaInstaller::NotifySodaDownloadProgressForTesting(int progress) {
  soda_binary_installed_ = false;
  is_soda_downloading_ = true;
  installed_languages_.clear();
  NotifyOnSodaProgress(progress);
}

void SodaInstaller::NotifyOnSodaLanguagePackInstalledForTesting(
    LanguageCode language_code) {
  installed_languages_.insert(language_code);
  auto it = language_pack_progress_.find(language_code);
  if (it != language_pack_progress_.end())
    language_pack_progress_.erase(language_code);
  NotifyOnSodaLanguagePackInstalled(language_code);
}

void SodaInstaller::NotifyOnSodaLanguagePackErrorForTesting(
    LanguageCode language_code) {
  auto it = language_pack_progress_.find(language_code);
  if (it != language_pack_progress_.end())
    language_pack_progress_.erase(language_code);
  NotifyOnSodaLanguagePackError(language_code);
}

void SodaInstaller::RegisterRegisteredLanguagePackPref(
    PrefRegistrySimple* registry) {
  // TODO: Default to one of the user's languages.
  base::Value::ListStorage default_languages;
  default_languages.push_back(base::Value(kUsEnglishLocale));
  registry->RegisterListPref(prefs::kSodaRegisteredLanguagePacks,
                             base::Value(std::move(default_languages)));
}

void SodaInstaller::NotifyOnSodaInstalled() {
  for (Observer& observer : observers_)
    observer.OnSodaInstalled();
}

void SodaInstaller::NotifyOnSodaLanguagePackInstalled(
    speech::LanguageCode language_code) {
  for (Observer& observer : observers_)
    observer.OnSodaLanguagePackInstalled(language_code);
}

void SodaInstaller::NotifyOnSodaError() {
  for (Observer& observer : observers_)
    observer.OnSodaError();
}

void SodaInstaller::NotifyOnSodaLanguagePackError(
    speech::LanguageCode language_code) {
  for (Observer& observer : observers_)
    observer.OnSodaLanguagePackError(language_code);
}

void SodaInstaller::NotifyOnSodaProgress(int combined_progress) {
  for (Observer& observer : observers_)
    observer.OnSodaProgress(combined_progress);
}

void SodaInstaller::NotifyOnSodaLanguagePackProgress(
    int language_progress,
    LanguageCode language_code) {
  for (Observer& observer : observers_)
    observer.OnSodaLanguagePackProgress(language_progress, language_code);
}

void SodaInstaller::RegisterLanguage(const std::string& language,
                                     PrefService* global_prefs) {
  ListPrefUpdate update(global_prefs, prefs::kSodaRegisteredLanguagePacks);
  if (!base::Contains(update->GetList(), base::Value(language))) {
    update->Append(language);
  }
}

void SodaInstaller::UnregisterLanguages(PrefService* global_prefs) {
  ListPrefUpdate update(global_prefs, prefs::kSodaRegisteredLanguagePacks);
  update->ClearList();
}

bool SodaInstaller::IsSodaDownloading(
    speech::LanguageCode language_code) const {
  return is_soda_downloading_ || language_pack_progress_.find(language_code) !=
                                     language_pack_progress_.end();
}

bool SodaInstaller::IsAnyFeatureUsingSodaEnabled(PrefService* prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1165437): Add Projector feature.
  return prefs->GetBoolean(prefs::kLiveCaptionEnabled) ||
         prefs->GetBoolean(ash::prefs::kAccessibilityDictationEnabled);
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
  return prefs->GetBoolean(prefs::kLiveCaptionEnabled);
#endif
}

}  // namespace speech
