// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/webui/projector_app/annotator_message_handler.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "media/base/media_switches.h"
#include "projector_client_impl.h"
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
  web_view->LoadInitialURL(GURL(ash::kChromeUITrustedAnnotatorUrl));

  content::WebContents* web_contents = web_view->GetWebContents();
  content::WebUI* web_ui = web_contents->GetWebUI();
  web_ui->AddMessageHandler(std::make_unique<ash::AnnotatorMessageHandler>());
}

ProjectorClientImpl::ProjectorClientImpl(ash::ProjectorController* controller)
    : controller_(controller) {
  controller_->SetClient(this);
}

ProjectorClientImpl::ProjectorClientImpl()
    : ProjectorClientImpl(ash::ProjectorController::Get()) {}

ProjectorClientImpl::~ProjectorClientImpl() = default;

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
  speech_recognizer_.reset();
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
}

void ProjectorClientImpl::ShowSelfieCam() {
  selfie_cam_bubble_manager_.Show(
      ProfileManager::GetActiveUserProfile(),
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
}

void ProjectorClientImpl::CloseSelfieCam() {
  selfie_cam_bubble_manager_.Close();
}

bool ProjectorClientImpl::IsSelfieCamVisible() const {
  return selfie_cam_bubble_manager_.IsVisible();
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
      drive::DriveIntegrationServiceFactory::FindForProfile(
          ProfileManager::GetActiveUserProfile());
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

  auto* profile = ProfileManager::GetActiveUserProfile();
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  return integration_service && integration_service->IsMounted();
}

void ProjectorClientImpl::OpenProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  web_app::LaunchSystemWebAppAsync(profile, web_app::SystemAppType::PROJECTOR);
}

void ProjectorClientImpl::MinimizeProjectorApp() const {
  auto* profile = ProfileManager::GetActiveUserProfile();
  auto* browser =
      FindSystemWebAppBrowser(profile, web_app::SystemAppType::PROJECTOR);
  if (browser)
    browser->window()->Minimize();
}

void ProjectorClientImpl::OnNewScreencastPreconditionChanged(
    const ash::NewScreencastPrecondition& precondition) const {
  ash::ProjectorAppClient::Get()->OnNewScreencastPreconditionChanged(
      precondition);
}
