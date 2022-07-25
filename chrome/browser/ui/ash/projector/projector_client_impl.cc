// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/annotator_tool.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/annotator_message_handler.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/bind.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "media/base/media_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

namespace {

inline const std::string& GetLocale() {
  return g_browser_process->GetApplicationLocale();
}

}  // namespace

// static
void ProjectorClientImpl::InitForProjectorAnnotator(views::WebView* web_view) {
  if (!ash::features::IsProjectorAnnotatorEnabled())
    return;
  web_view->LoadInitialURL(GURL(ash::kChromeUITrustedAnnotatorAppUrl));
}

ProjectorClientImpl::ProjectorClientImpl(ash::ProjectorController* controller)
    : controller_(controller) {
  controller_->SetClient(this);
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (session_manager)
    session_observation_.Observe(session_manager);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager)
    session_state_observation_.Observe(user_manager);
}

ProjectorClientImpl::ProjectorClientImpl()
    : ProjectorClientImpl(ash::ProjectorController::Get()) {}

ProjectorClientImpl::~ProjectorClientImpl() {
  controller_->SetClient(nullptr);
}

void ProjectorClientImpl::StartSpeechRecognition() {
  // ProjectorController should only request for speech recognition after it
  // has been informed that recognition is available.
  // TODO(crbug.com/1165437): Dynamically determine language code.
  DCHECK(OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
      GetLocale()));

  DCHECK_EQ(speech_recognizer_.get(), nullptr);
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
  speech_recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
      weak_ptr_factory_.GetWeakPtr(), ProfileManager::GetActiveUserProfile(),
      GetLocale(), /*recognition_mode_ime=*/false,
      /*enable_formatting=*/true);
}

void ProjectorClientImpl::StopSpeechRecognition() {
  speech_recognizer_->Stop();
}

bool ProjectorClientImpl::GetDriveFsMountPointPath(
    base::FilePath* result) const {
  if (!IsDriveFsMounted())
    return false;

  if (ash::ProjectorController::AreExtendedProjectorFeaturesDisabled()) {
    auto* profile = ProfileManager::GetActiveUserProfile();
    DCHECK(profile);

    DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
        ProfileManager::GetActiveUserProfile());
    *result = download_prefs->GetDefaultDownloadDirectoryForProfile();
    return true;
  }

  drive::DriveIntegrationService* integration_service =
      GetDriveIntegrationServiceForActiveProfile();
  *result = integration_service->GetMountPointPath();
  return true;
}

bool ProjectorClientImpl::IsDriveFsMounted() const {
  if (!chromeos::LoginState::Get()->IsUserLoggedIn())
    return false;

  if (ash::ProjectorController::AreExtendedProjectorFeaturesDisabled()) {
    // Return true when extended projector features are disabled. Use download
    // folder for Projector storage.
    return true;
  }

  drive::DriveIntegrationService* integration_service =
      GetDriveIntegrationServiceForActiveProfile();
  return integration_service && integration_service->IsMounted();
}

bool ProjectorClientImpl::IsDriveFsMountFailed() const {
  drive::DriveIntegrationService* integration_service =
      GetDriveIntegrationServiceForActiveProfile();
  return integration_service && integration_service->mount_failed();
}

void ProjectorClientImpl::OpenProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  web_app::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::PROJECTOR);
}

void ProjectorClientImpl::MinimizeProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto* browser = web_app::FindSystemWebAppBrowser(
      profile, ash::SystemWebAppType::PROJECTOR);
  if (browser)
    browser->window()->Minimize();
}

void ProjectorClientImpl::CloseProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto* browser = web_app::FindSystemWebAppBrowser(
      profile, ash::SystemWebAppType::PROJECTOR);
  if (browser)
    browser->window()->Close();
}

void ProjectorClientImpl::OnNewScreencastPreconditionChanged(
    const ash::NewScreencastPrecondition& precondition) const {
  ash::ProjectorAppClient* app_client = ash::ProjectorAppClient::Get();
  if (app_client)
    app_client->OnNewScreencastPreconditionChanged(precondition);
}

void ProjectorClientImpl::SetAnnotatorMessageHandler(
    ash::AnnotatorMessageHandler* handler) {
  message_handler_ = handler;
}

void ProjectorClientImpl::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const absl::optional<media::SpeechRecognitionResult>& full_result) {
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
  speech_recognizer_.reset();
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
  controller_->OnSpeechRecognitionStopped();
}

void ProjectorClientImpl::SetTool(const ash::AnnotatorTool& tool) {
  message_handler_->SetTool(tool);
}

// TODO(b/220202359): Implement undo.
void ProjectorClientImpl::Undo() {}

// TODO(b/220202359): Implement redo.
void ProjectorClientImpl::Redo() {}

void ProjectorClientImpl::Clear() {
  message_handler_->Clear();
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

void ProjectorClientImpl::OnUserProfileLoaded(const AccountId& account_id) {
  MaybeSwitchDriveIntegrationServiceObservation();
}

void ProjectorClientImpl::OnUserSessionStarted(bool is_primary_user) {
  if (!is_primary_user || !pref_change_registrar_.IsEmpty())
    return;
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

void ProjectorClientImpl::ActiveUserChanged(user_manager::User* active_user) {
  // After user login, the first ActiveUserChanged() might be called before
  // profile is loaded.
  if (active_user->is_profile_created())
    MaybeSwitchDriveIntegrationServiceObservation();
}

void ProjectorClientImpl::MaybeSwitchDriveIntegrationServiceObservation() {
  auto* profile = ProfileManager::GetActiveUserProfile();
  if (!IsProjectorAllowedForProfile(profile))
    return;

  drive::DriveIntegrationService* drive_service =
      GetDriveIntegrationServiceForActiveProfile();
  if (!drive_service || drive_observation_.IsObservingSource(drive_service))
    return;

  drive_observation_.Reset();
  drive_observation_.Observe(drive_service);
}

void ProjectorClientImpl::OnEnablementPolicyChanged() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);
  const bool is_installed =
      swa_manager &&
      swa_manager->IsSystemWebApp(ash::kChromeUITrustedProjectorSwaAppId);
  // We can't enable or disable the app if it's not already installed.
  if (!is_installed)
    return;

  const bool is_enabled = IsProjectorAppEnabled(profile);
  // The policy has changed to disallow the Projector app. Since we can't
  // uninstall the Projector SWA until the user signs out and back in, we should
  // close and disable the app for this current session.
  if (!is_enabled)
    CloseProjectorApp();

  auto* web_app_provider = ash::SystemWebAppManager::GetWebAppProvider(profile);
  web_app_provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&ProjectorClientImpl::SetAppIsDisabled,
                                weak_ptr_factory_.GetWeakPtr(),
                                web_app_provider, !is_enabled));
}

void ProjectorClientImpl::SetAppIsDisabled(web_app::WebAppProvider* provider,
                                           bool disabled) {
  provider->sync_bridge().SetAppIsDisabled(
      ash::kChromeUITrustedProjectorSwaAppId, disabled);
}
