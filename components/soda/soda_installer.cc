// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_installer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/ash_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

SodaInstaller::SodaInstaller() = default;

SodaInstaller::~SodaInstaller() = default;

void SodaInstaller::Init(PrefService* profile_prefs,
                         PrefService* global_prefs) {
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) ||
      soda_installer_initialized_) {
    return;
  }

  if (IsAnyFeatureUsingSodaEnabled(profile_prefs)) {
    soda_installer_initialized_ = true;
    global_prefs->SetTime(prefs::kSodaScheduledDeletionTime, base::Time());
    speech::SodaInstaller::GetInstance()->InstallSoda(global_prefs);
    for (const auto& language :
         global_prefs->GetList(prefs::kSodaRegisteredLanguagePacks)
             ->GetList()) {
      speech::SodaInstaller::GetInstance()->InstallLanguage(
          language.GetString(), global_prefs);
    }
  } else {
    base::Time deletion_time =
        global_prefs->GetTime(prefs::kSodaScheduledDeletionTime);
    if (!deletion_time.is_null() && deletion_time < base::Time::Now()) {
      UninstallSoda(global_prefs);
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

void SodaInstaller::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SodaInstaller::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SodaInstaller::NotifySodaInstalledForTesting() {
  soda_binary_installed_ = true;
  language_installed_ = true;
  NotifyOnSodaInstalled();
}

void SodaInstaller::RegisterRegisteredLanguagePackPref(
    PrefRegistrySimple* registry) {
  // TODO: Default to one of the user's languages.
  base::Value::ListStorage default_languages;
  default_languages.push_back(base::Value("en-US"));
  registry->RegisterListPref(prefs::kSodaRegisteredLanguagePacks,
                             base::Value(std::move(default_languages)));
}

void SodaInstaller::NotifyOnSodaInstalled() {
  for (Observer& observer : observers_)
    observer.OnSodaInstalled();
}

void SodaInstaller::NotifyOnSodaLanguagePackInstalled(
    speech::LanguageCode language_code) {
  if (base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    for (Observer& observer : observers_)
      observer.OnSodaLanguagePackInstalled(language_code);
  }
}

void SodaInstaller::NotifyOnSodaError() {
  for (Observer& observer : observers_)
    observer.OnSodaError();
}

void SodaInstaller::NotifyOnSodaLanguagePackError(
    speech::LanguageCode language_code) {
  if (base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    for (Observer& observer : observers_)
      observer.OnSodaLanguagePackError(language_code);
  }
}

void SodaInstaller::NotifyOnSodaProgress(int combined_progress) {
  for (Observer& observer : observers_)
    observer.OnSodaProgress(combined_progress);
}

void SodaInstaller::NotifyOnSodaLanguagePackProgress(
    int language_progress,
    LanguageCode language_code) {
  if (base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    for (Observer& observer : observers_)
      observer.OnSodaLanguagePackProgress(language_progress, language_code);
  }
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
  update->Clear();
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
