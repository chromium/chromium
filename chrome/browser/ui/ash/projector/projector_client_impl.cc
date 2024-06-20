// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/speech_recognition_recognizer_client_impl.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace {

constexpr char kUSMExperimentRoutingId[] = "screencast_usm_rnnt";

inline const std::string& GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

inline const std::string GetLocaleOrLanguageForServerSideRecognition() {
  const std::string& locale = g_browser_process->GetApplicationLocale();
  // Some languages and locales need to be mapped to the default
  // languages/locales provided by the server side speech recognition service.
  if (locale == "ar") {
    return "ar-x-maghrebi";
  }
  return locale;
}

ash::OnDeviceToServerSpeechRecognitionFallbackReason GetFallbackReason(
    ash::OnDeviceRecognitionAvailability availability) {
  if (ash::features::ShouldForceEnableServerSideSpeechRecognitionForDev()) {
    return ash::OnDeviceToServerSpeechRecognitionFallbackReason::
        kEnforcedByFlag;
  }

  DCHECK_NE(availability, ash::OnDeviceRecognitionAvailability::kAvailable);
  switch (availability) {
    case ash::OnDeviceRecognitionAvailability::kSodaNotAvailable:
      return ash::OnDeviceToServerSpeechRecognitionFallbackReason::
          kSodaNotAvailable;
    case ash::OnDeviceRecognitionAvailability::kUserLanguageNotAvailable:
      return ash::OnDeviceToServerSpeechRecognitionFallbackReason::
          kUserLanguageNotAvailableForSoda;
    case ash::OnDeviceRecognitionAvailability::kSodaNotInstalled:
      return ash::OnDeviceToServerSpeechRecognitionFallbackReason::
          kSodaNotInstalled;
    case ash::OnDeviceRecognitionAvailability::kSodaInstalling:
      return ash::OnDeviceToServerSpeechRecognitionFallbackReason::
          kSodaInstalling;
    case ash::OnDeviceRecognitionAvailability::
        kSodaInstallationErrorUnspecified:
      return ash::OnDeviceToServerSpeechRecognitionFallbackReason::
          kSodaInstallationErrorUnspecified;
    case ash::OnDeviceRecognitionAvailability::
        kSodaInstallationErrorNeedsReboot:
      return ash::OnDeviceToServerSpeechRecognitionFallbackReason::
          kSodaInstallationErrorNeedsReboot;
    case ash::OnDeviceRecognitionAvailability::kAvailable:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return ash::OnDeviceToServerSpeechRecognitionFallbackReason::kMaxValue;
}

}  // namespace

// Using base::Unretained for callback is safe since the ProjectorClientImpl
// owns `drive_helper_`.
ProjectorClientImpl::ProjectorClientImpl(ash::ProjectorController* controller)
    : controller_(controller),
      drive_helper_(base::BindRepeating(
          &ProjectorClientImpl::MaybeSwitchDriveIntegrationServiceObservation,
          base::Unretained(this))) {
  controller_->SetClient(this);
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (session_manager) {
    session_observation_.Observe(session_manager);
  }

  if (base::FeatureList::IsEnabled(ash::features::kOnDeviceSpeechRecognition)) {
    soda_installation_controller_ =
        std::make_unique<ProjectorSodaInstallationController>(
            ash::ProjectorAppClient::Get(), controller_);
  }
}

ProjectorClientImpl::ProjectorClientImpl()
    : ProjectorClientImpl(ash::ProjectorController::Get()) {}

ProjectorClientImpl::~ProjectorClientImpl() {
  controller_->SetClient(nullptr);
}

// Projector prioritizes on-device speech recognition over server
// based speech recognition.
ash::SpeechRecognitionAvailability
ProjectorClientImpl::GetSpeechRecognitionAvailability() const {
  ash::SpeechRecognitionAvailability availability;
  availability.use_on_device = true;
  availability.on_device_availability = SpeechRecognitionRecognizerClientImpl::
      GetOnDeviceSpeechRecognitionAvailability(GetLocale());
  availability.server_based_availability =
      SpeechRecognitionRecognizerClientImpl::
          GetServerBasedRecognitionAvailability(
              GetLocaleOrLanguageForServerSideRecognition());

  if (ash::features::ShouldForceEnableServerSideSpeechRecognitionForDev() ||
      (availability.on_device_availability !=
           ash::OnDeviceRecognitionAvailability::kAvailable &&
       availability.server_based_availability ==
           ash::ServerBasedRecognitionAvailability::kAvailable)) {
    availability.use_on_device = false;
  }

  return availability;
}

void ProjectorClientImpl::StartSpeechRecognition() {
  const auto availability = GetSpeechRecognitionAvailability();
  DCHECK(availability.IsAvailable());
  DCHECK_EQ(speech_recognizer_.get(), nullptr);
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
  const std::string locale =
      availability.use_on_device
          ? GetLocale()
          : GetLocaleOrLanguageForServerSideRecognition();
  const std::string experiment_recognizer_routing_key =
      ash::features::IsProjectorUseUSMForS3Enabled() ? kUSMExperimentRoutingId
                                                     : "";

  speech_recognizer_ = std::make_unique<SpeechRecognitionRecognizerClientImpl>(
      weak_ptr_factory_.GetWeakPtr(), ProfileManager::GetActiveUserProfile(),
      media::AudioDeviceDescription::kDefaultDeviceId,
      media::mojom::SpeechRecognitionOptions::New(
          media::mojom::SpeechRecognitionMode::kCaption,
          /*enable_formatting=*/true, locale,
          /*is_server_based=*/!availability.use_on_device,
          media::mojom::RecognizerClientType::kProjector,
          /*skip_continuously_empty_audio=*/false,
          experiment_recognizer_routing_key));
  if (!availability.use_on_device) {
    RecordOnDeviceToServerSpeechRecognitionFallbackReason(
        GetFallbackReason(availability.on_device_availability));
  }
}

void ProjectorClientImpl::StopSpeechRecognition() {
  if (!speech_recognizer_) {
    LOG(ERROR) << "Stop was called on a destroyed speech recognizer.";
    return;
  }

  speech_recognizer_->Stop();
}

void ProjectorClientImpl::ForceEndSpeechRecognition() {
  SpeechRecognitionEnded(/*forced=*/true);
}

bool ProjectorClientImpl::GetBaseStoragePath(base::FilePath* result) const {
  if (!IsDriveFsMounted()) {
    return false;
  }

  if (ash::ProjectorController::AreExtendedProjectorFeaturesDisabled()) {
    auto* profile = ProfileManager::GetActiveUserProfile();
    DCHECK(profile);

    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
        ProfileManager::GetActiveUserProfile());
    *result = download_prefs->GetDefaultDownloadDirectoryForProfile();
    return true;
  }

  *result = ProjectorDriveFsProvider::GetDriveFsMountPointPath();
  return true;
}

bool ProjectorClientImpl::IsDriveFsMounted() const {
  if (!ash::LoginState::Get()->IsUserLoggedIn()) {
    return false;
  }

  if (ash::ProjectorController::AreExtendedProjectorFeaturesDisabled()) {
    // Return true when extended projector features are disabled. Use download
    // folder for Projector storage.
    return true;
  }
  return ProjectorDriveFsProvider::IsDriveFsMounted();
}

bool ProjectorClientImpl::IsDriveFsMountFailed() const {
  return ProjectorDriveFsProvider::IsDriveFsMountFailed();
}

void ProjectorClientImpl::OpenProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::PROJECTOR);
}

void ProjectorClientImpl::MinimizeProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto* browser =
      ash::FindSystemWebAppBrowser(profile, ash::SystemWebAppType::PROJECTOR);
  if (browser) {
    browser->window()->Minimize();
  }
}

void ProjectorClientImpl::CloseProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto* browser =
      ash::FindSystemWebAppBrowser(profile, ash::SystemWebAppType::PROJECTOR);
  if (browser) {
    browser->window()->Close();
  }
}

void ProjectorClientImpl::OnNewScreencastPreconditionChanged(
    const ash::NewScreencastPrecondition& precondition) const {
  ash::ProjectorAppClient* app_client = ash::ProjectorAppClient::Get();
  if (app_client) {
    app_client->OnNewScreencastPreconditionChanged(precondition);
  }
}

void ProjectorClientImpl::ToggleFileSyncingNotificationForPaths(
    const std::vector<base::FilePath>& screencast_paths,
    bool suppress) {
  if (auto* app_client = ash::ProjectorAppClient::Get()) {
    app_client->ToggleFileSyncingNotificationForPaths(screencast_paths,
                                                      suppress);
  }
}

void ProjectorClientImpl::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const std::optional<media::SpeechRecognitionResult>& full_result) {
  DCHECK(full_result.has_value());
  controller_->OnTranscription(full_result.value());
}

void ProjectorClientImpl::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  if (new_state == SPEECH_RECOGNIZER_ERROR) {
    speech_recognizer_.reset();
    recognizer_status_ = SPEECH_RECOGNIZER_OFF;
    controller_->OnTranscriptionError();
  } else if (new_state == SPEECH_RECOGNIZER_READY) {
    if (recognizer_status_ == SPEECH_RECOGNIZER_OFF && speech_recognizer_) {
      // The SpeechRecognizer was initialized after being created, and
      // is ready to start recognizing speech.
      speech_recognizer_->Start();
    }
  }

  recognizer_status_ = new_state;
}

void ProjectorClientImpl::OnSpeechRecognitionStopped() {
  SpeechRecognitionEnded(/*forced=*/false);
}

void ProjectorClientImpl::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  // For now, this is ignored by projector.
}

void ProjectorClientImpl::OnFileSystemMounted() {
  OnNewScreencastPreconditionChanged(
      controller_->GetNewScreencastPrecondition());
}

void ProjectorClientImpl::OnFileSystemBeingUnmounted() {
  OnNewScreencastPreconditionChanged(
      controller_->GetNewScreencastPrecondition());
}

void ProjectorClientImpl::OnFileSystemMountFailed() {
  OnNewScreencastPreconditionChanged(
      controller_->GetNewScreencastPrecondition());
}

void ProjectorClientImpl::OnUserSessionStarted(bool is_primary_user) {
  if (!is_primary_user || !pref_change_registrar_.IsEmpty()) {
    return;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  pref_change_registrar_.Init(profile->GetPrefs());
  // TOOD(b/232043809): Consider using the disabled system feature policy
  // instead.
  pref_change_registrar_.Add(
      ash::prefs::kProjectorAllowByPolicy,
      base::BindRepeating(&ProjectorClientImpl::OnEnablementPolicyChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::prefs::kProjectorDogfoodForFamilyLinkEnabled,
      base::BindRepeating(&ProjectorClientImpl::OnEnablementPolicyChanged,
                          base::Unretained(this)));
}

void ProjectorClientImpl::MaybeSwitchDriveIntegrationServiceObservation() {
  if (drive::DriveIntegrationService* const service =
          ProjectorDriveFsProvider::GetActiveDriveIntegrationService()) {
    Observe(service);
  }
}

void ProjectorClientImpl::SpeechRecognitionEnded(bool forced) {
  speech_recognizer_.reset();
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
  controller_->OnSpeechRecognitionStopped(forced);
}

void ProjectorClientImpl::OnEnablementPolicyChanged() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);
  CHECK(swa_manager);
  const bool is_installed =
      swa_manager->IsSystemWebApp(ash::kChromeUIUntrustedProjectorSwaAppId);
  // We can't enable or disable the app if it's not already installed.
  if (!is_installed) {
    return;
  }

  const bool is_enabled = IsProjectorAppEnabled(profile);
  // The policy has changed to disallow the Projector app. Since we can't
  // uninstall the Projector SWA until the user signs out and back in, we should
  // close and disable the app for this current session.
  if (!is_enabled) {
    CloseProjectorApp();
  }

  auto* web_app_provider = ash::SystemWebAppManager::GetWebAppProvider(profile);
  CHECK(web_app_provider);
  web_app_provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&ProjectorClientImpl::SetAppIsDisabled,
                                weak_ptr_factory_.GetWeakPtr(), !is_enabled));
}

void ProjectorClientImpl::SetAppIsDisabled(bool disabled) {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  auto* web_app_provider = ash::SystemWebAppManager::GetWebAppProvider(profile);
  CHECK(web_app_provider);

  web_app_provider->scheduler().SetAppIsDisabled(
      ash::kChromeUIUntrustedProjectorSwaAppId, disabled, base::DoNothing());
}
