// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_

#include <memory>

#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chrome/browser/ui/webui/chromeos/projector/selfie_cam_bubble_manager.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"

namespace views {
class WebView;
}  // namespace views

class OnDeviceSpeechRecognizer;

// The client implementation for the ProjectorController in ash/. This client is
// responsible for handling requests that have browser dependencies.
class ProjectorClientImpl : public ash::ProjectorClient,
                            public SpeechRecognizerDelegate {
 public:
  // RecordingOverlayViewImpl calls this function to initialize the annotator
  // tool.
  static void InitForProjectorAnnotator(views::WebView* web_view);

  explicit ProjectorClientImpl(ash::ProjectorController* controller);

  ProjectorClientImpl();
  ProjectorClientImpl(const ProjectorClientImpl&) = delete;
  ProjectorClientImpl& operator=(const ProjectorClientImpl&) = delete;
  ~ProjectorClientImpl() override;

  // ash::ProjectorClient:
  void StartSpeechRecognition() override;
  void StopSpeechRecognition() override;
  void ShowSelfieCam() override;
  void CloseSelfieCam() override;
  bool IsSelfieCamVisible() const override;
  bool GetDriveFsMountPointPath(base::FilePath* result) const override;
  bool IsDriveFsMounted() const override;
  void OpenProjectorApp() const override;
  void MinimizeProjectorApp() const override;
  void OnNewScreencastPreconditionChanged(
      const ash::NewScreencastPrecondition& precondition) const override;

  // SpeechRecognizerDelegate:
  void OnSpeechResult(
      const std::u16string& text,
      bool is_final,
      const absl::optional<media::SpeechRecognitionResult>& timing) override;
  // This class is not utilizing the information about sound level.
  void OnSpeechSoundLevelChanged(int16_t level) override {}
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;

 private:
  ash::ProjectorController* const controller_;
  SpeechRecognizerStatus recognizer_status_ =
      SpeechRecognizerStatus::SPEECH_RECOGNIZER_OFF;
  std::unique_ptr<OnDeviceSpeechRecognizer> speech_recognizer_;
  chromeos::SelfieCamBubbleManager selfie_cam_bubble_manager_;
  base::WeakPtrFactory<ProjectorClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_
