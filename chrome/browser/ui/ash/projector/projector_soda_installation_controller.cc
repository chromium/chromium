// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_soda_installation_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/speech_recognition_recognizer_client_impl.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"

namespace {

inline const std::string& GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

inline bool IsLanguageSupported(const speech::LanguageCode languageCode) {
  auto* soda_installer = speech::SodaInstaller::GetInstance();
  for (auto const& language : soda_installer->GetAvailableLanguages()) {
    if (speech::GetLanguageCode(language) == languageCode)
      return true;
  }
  return false;
}

}  // namespace

ProjectorSodaInstallationController::ProjectorSodaInstallationController(
    ash::ProjectorAppClient* client,
    ash::ProjectorController* projector_controller)
    : app_client_(client), projector_controller_(projector_controller) {
  soda_installer_observation_.Observe(speech::SodaInstaller::GetInstance());
  locale_change_observation_.Observe(ash::LocaleUpdateController::Get());
}

ProjectorSodaInstallationController::~ProjectorSodaInstallationController() =
    default;

void ProjectorSodaInstallationController::InstallSoda(
    const std::string& language) {
  auto languageCode = speech::GetLanguageCode(language);
  auto* soda_installer = speech::SodaInstaller::GetInstance();

  // Initialization will trigger the installation of SODA and the language.
  PrefService* pref_service =
      ProfileManager::GetPrimaryUserProfile()->GetPrefs();
  pref_service->SetString(ash::prefs::kProjectorCreationFlowLanguage, language);
  soda_installer->Init(pref_service, g_browser_process->local_state());

  if (!soda_installer->IsSodaDownloading(languageCode))
    soda_installer->InstallLanguage(language, g_browser_process->local_state());
}

bool ProjectorSodaInstallationController::ShouldDownloadSoda(
    speech::LanguageCode language_code) {
  return base::FeatureList::IsEnabled(
             ash::features::kOnDeviceSpeechRecognition) &&
         IsLanguageSupported(language_code) && !IsSodaAvailable(language_code);
}

bool ProjectorSodaInstallationController::IsSodaAvailable(
    speech::LanguageCode language_code) {
  return speech::SodaInstaller::GetInstance()->IsSodaInstalled(language_code);
}

void ProjectorSodaInstallationController::OnSodaInstalled(
    speech::LanguageCode language_code) {
  // Check that language code matches the selected language for projector.
  if (language_code != speech::GetLanguageCode(GetLocale()))
    return;
  projector_controller_->OnSpeechRecognitionAvailabilityChanged();
  app_client_->OnSodaInstalled();
}

void ProjectorSodaInstallationController::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  const auto& current_locale = GetLocale();
  // Check that language code matches the selected language for projector or is
  // LanguageCode::kNone (signifying the SODA binary failed).
  if (language_code != speech::GetLanguageCode(current_locale) &&
      language_code != speech::LanguageCode::kNone) {
    return;
  }
  projector_controller_->OnSpeechRecognitionAvailabilityChanged();

  if (SpeechRecognitionRecognizerClientImpl::
          GetServerBasedRecognitionAvailability(current_locale) !=
      ash::ServerBasedRecognitionAvailability::kAvailable) {
    app_client_->OnSodaInstallError();
  }
}

void ProjectorSodaInstallationController::OnSodaProgress(
    speech::LanguageCode language_code,
    int progress) {
  // Check that language code matches the selected language for projector or is
  // LanguageCode::kNone (signifying the SODA binary has progress).
  if (language_code != speech::GetLanguageCode(GetLocale()) &&
      language_code != speech::LanguageCode::kNone) {
    return;
  }
  app_client_->OnSodaInstallProgress(progress);
}

// This function is triggered after every sign in.
void ProjectorSodaInstallationController::OnLocaleChanged() {
  if (!IsLanguageSupported(speech::GetLanguageCode(GetLocale()))) {
    projector_controller_->OnSpeechRecognitionAvailabilityChanged();
  }
}
