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

namespace {
// On-device speech recognition is only available in US English.
const char kEnglishLanguageCode[] = "en-US";

bool ShouldUseWebSpeechFallback() {
  return !base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption);
}

}  // namespace

ProjectorClientImpl::ProjectorClientImpl() {
  ash::ProjectorController::Get()->SetClient(this);

  bool recognition_available =
      OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
          kEnglishLanguageCode) ||
      ShouldUseWebSpeechFallback();

  ash::ProjectorController::Get()->OnSpeechRecognitionAvailable(
      recognition_available);
  if (!recognition_available) {
    observed_soda_installer_.Observe(speech::SodaInstaller::GetInstance());
  }
}

ProjectorClientImpl::~ProjectorClientImpl() = default;

void ProjectorClientImpl::StartSpeechRecognition() {
  // ProjectorController should only request for speech recognition after it
  // has been informed that recognition is available.
  // TODO(crbug.com/1165437): Dynamically determine language code.
  DCHECK(OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
             kEnglishLanguageCode) ||
         ShouldUseWebSpeechFallback());
  DCHECK_EQ(speech_recognizer_.get(), nullptr);
  speech_recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
      weak_ptr_factory_.GetWeakPtr(), ProfileManager::GetPrimaryUserProfile(),
      kEnglishLanguageCode, /*recognition_mode_ime=*/false);
  speech_recognizer_->Start();
}

void ProjectorClientImpl::StopSpeechRecognition() {
  speech_recognizer_.reset();
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
}

void ProjectorClientImpl::ShowSelfieCam() {
  selfie_cam_bubble_manager_.Show(ProfileManager::GetPrimaryUserProfile());
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
    const absl::optional<SpeechRecognizerDelegate::TranscriptTiming>& timing) {
  DCHECK(timing.has_value() || ShouldUseWebSpeechFallback());

  if (timing.has_value()) {
    ash::ProjectorController::Get()->OnTranscription(
        text, timing->audio_start_time, timing->audio_end_time,
        timing->word_offsets, is_final);
  } else {
    // This is only used for development.
    ash::ProjectorController::Get()->OnTranscription(
        text, absl::nullopt, absl::nullopt, absl::nullopt, is_final);
  }
}

void ProjectorClientImpl::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  recognizer_status_ = new_state;
  // TODO(yilkal): Handle the new state appropriately.
}

void ProjectorClientImpl::OnSodaInstalled() {
  // OnDevice has been installed! Notify ProjectorController in ash.
  DCHECK(OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
      kEnglishLanguageCode));
  ash::ProjectorController::Get()->OnSpeechRecognitionAvailable(true);
}
