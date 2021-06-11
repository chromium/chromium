// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include "ash/public/cpp/projector/projector_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"
#include "components/soda/soda_installer.h"
#include "media/base/media_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {
// On-device speech recognition is only available in US English.
const char kEnglishLanguageCode[] = "en-US";

bool ShouldUseWebSpeechFallback() {
  return !base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption);
}

}  // namespace

ProjectorClientImpl::ProjectorClientImpl(ash::ProjectorController* controller)
    : controller_(controller) {
  controller_->SetClient(this);
  bool recognition_available =
      OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
          kEnglishLanguageCode) ||
      ShouldUseWebSpeechFallback();

  controller_->OnSpeechRecognitionAvailable(recognition_available);
  if (!recognition_available) {
    speech::SodaInstaller::GetInstance()->AddObserver(this);
  }
}

ProjectorClientImpl::ProjectorClientImpl()
    : ProjectorClientImpl(ash::ProjectorController::Get()) {}

ProjectorClientImpl::~ProjectorClientImpl() = default;

void ProjectorClientImpl::StartSpeechRecognition() {
  // ProjectorController should only request for speech recognition after it
  // has been informed that recognition is available.
  // TODO(crbug.com/1165437): Dynamically determine language code.
  DCHECK(OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
             kEnglishLanguageCode) ||
         ShouldUseWebSpeechFallback());
  DCHECK_EQ(speech_recognizer_.get(), nullptr);
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
  speech_recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
      weak_ptr_factory_.GetWeakPtr(), ProfileManager::GetPrimaryUserProfile(),
      kEnglishLanguageCode, /*recognition_mode_ime=*/false,
      /*enable_formatting=*/true);
}

void ProjectorClientImpl::StopSpeechRecognition() {
  speech_recognizer_.reset();
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
}

void ProjectorClientImpl::ShowSelfieCam() {
  selfie_cam_bubble_manager_.Show(
      ProfileManager::GetPrimaryUserProfile(),
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

void ProjectorClientImpl::OnSodaInstalled() {
  // OnDevice has been installed! Notify ProjectorController in ash.
  DCHECK(OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
      kEnglishLanguageCode));
  controller_->OnSpeechRecognitionAvailable(true);
}
