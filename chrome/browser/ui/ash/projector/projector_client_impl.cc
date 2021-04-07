// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include "ash/public/cpp/projector/projector_controller.h"
#include "base/optional.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"

namespace {
// On-device speech recognition is only available in US English.
const char kEnglishLanguageCode[] = "en-US";
}  // namespace

ProjectorClientImpl::ProjectorClientImpl() {
  ash::ProjectorController::Get()->SetClient(this);
  bool soda_available =
      OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
          kEnglishLanguageCode);

  ash::ProjectorController::Get()->OnSpeechRecognitionAvailable(soda_available);
  if (!soda_available) {
    observed_soda_installer_.Observe(speech::SodaInstaller::GetInstance());
  }
}

ProjectorClientImpl::~ProjectorClientImpl() = default;

void ProjectorClientImpl::StartSpeechRecognition() {
  // ProjectorController should only request for speech recognition after it
  // has been informed that recognition is available.
  // TODO(crbug.com/1165437): Dynamically determine language code.
  DCHECK(OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(
      kEnglishLanguageCode));
  DCHECK_EQ(speech_recognizer_.get(), nullptr);
  speech_recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
      weak_ptr_factory_.GetWeakPtr(), ProfileManager::GetPrimaryUserProfile(),
      kEnglishLanguageCode);
  speech_recognizer_->Start();
}

void ProjectorClientImpl::StopSpeechRecognition() {
  speech_recognizer_.reset();
  recognizer_status_ = SPEECH_RECOGNIZER_OFF;
}

void ProjectorClientImpl::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const base::Optional<SpeechRecognizerDelegate::TranscriptTiming>& timing) {
  DCHECK(timing.has_value());
  ash::ProjectorController::Get()->OnTranscription(
      text, timing->audio_start_time, timing->audio_end_time,
      timing->word_offsets, is_final);
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
