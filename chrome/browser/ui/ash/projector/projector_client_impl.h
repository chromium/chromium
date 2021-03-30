// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_

#include <memory>

#include "ash/public/cpp/projector/projector_client.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"

class OnDeviceSpeechRecognizer;

// The client implementation for the ProjectorController in ash/. This client is
// responsible for handling requests that have browser dependencies.
class ProjectorClientImpl : public ash::ProjectorClient,
                            public SpeechRecognizerDelegate,
                            public speech::SodaInstaller::Observer {
 public:
  ProjectorClientImpl();
  ProjectorClientImpl(const ProjectorClientImpl&) = delete;
  ProjectorClientImpl& operator=(const ProjectorClientImpl&) = delete;
  ~ProjectorClientImpl() override;

  // ash::ProjectorClient:
  void StartSpeechRecognition() override;

  void StopSpeechRecognition() override;

  // SpeechRecognizerDelegate:
  void OnSpeechResult(
      const std::u16string& text,
      bool is_final,
      const base::Optional<SpeechRecognizerDelegate::TranscriptTiming>& timing)
      override;
  // This class is not utilizing the information about sound level.
  void OnSpeechSoundLevelChanged(int16_t level) override {}
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;

  // speech::SodaIntaller::Observer:
  void OnSodaInstalled() override;
  // We are not utilizing the following methods. Mark them as empty overrides.
  void OnSodaError() override {}
  void OnSodaProgress(int progress) override {}

 private:
  SpeechRecognizerStatus recognizer_status_ =
      SpeechRecognizerStatus::SPEECH_RECOGNIZER_OFF;
  base::ScopedObservation<speech::SodaInstaller,
                          speech::SodaInstaller::Observer>
      observed_soda_installer_{this};
  std::unique_ptr<OnDeviceSpeechRecognizer> speech_recognizer_;
  base::WeakPtrFactory<ProjectorClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_
