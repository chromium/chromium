// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_CAPTION_CONTROLLER_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_CAPTION_CONTROLLER_DELEGATE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/boca/babelorca/caption_controller.h"

class PrefService;

namespace captions {
class CaptionBubbleController;
}  // namespace captions

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ui {

class NativeThemeObserver;
struct CaptionStyle;

}  // namespace ui

namespace ash::babelorca {

class FakeCaptionControllerDelegate : public CaptionController::Delegate {
 public:
  FakeCaptionControllerDelegate();
  ~FakeCaptionControllerDelegate() override;

  std::unique_ptr<captions::CaptionBubbleController>
  CreateCaptionBubbleController(PrefService*, const std::string&) override;

  void AddCaptionStyleObserver(ui::NativeThemeObserver* observer) override;

  void RemoveCaptionStyleObserver(ui::NativeThemeObserver* observer) override;

  size_t GetCreateBubbleControllerCount();

  ui::NativeThemeObserver* GetCaptionStyleObserver();

  size_t GetOnAudioStreamEndCount();

  const std::vector<std::optional<ui::CaptionStyle>>&
  GetUpdateCaptionStyleUpdates();

  const std::vector<media::SpeechRecognitionResult>& GetTranscriptions();

  bool AddTranscription(media::SpeechRecognitionResult transcription);

  void SetOnTranscriptionSuccess(bool success);

  void OnAudioStreamEnd();

  void AddStyleUpdate(std::optional<ui::CaptionStyle> style);

  bool IsCaptionBubbleAlive();

  void SetCaptionBubbleDestroyed();

 private:
  bool caption_bubble_alive_ = false;
  bool on_transcritption_success_ = true;
  size_t create_bubble_controller_count_ = 0;
  size_t audio_stream_end_count_ = 0;
  std::vector<std::optional<ui::CaptionStyle>> caption_style_updates_;
  std::vector<media::SpeechRecognitionResult> transcriptions_;
  raw_ptr<ui::NativeThemeObserver> style_observer_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_CAPTION_CONTROLLER_DELEGATE_H_
