// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/fakes/fake_caption_controller_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chromeos/ash/components/boca/babelorca/caption_controller.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/prefs/pref_service.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ash::babelorca {
namespace {

class FakeCaptionBubbleController : public captions::CaptionBubbleController {
 public:
  explicit FakeCaptionBubbleController(FakeCaptionControllerDelegate* delegate)
      : delegate_(delegate) {}

  ~FakeCaptionBubbleController() override {
    delegate_->SetCaptionBubbleDestroyed();
  }

  bool OnTranscription(captions::CaptionBubbleContext*,
                       const media::SpeechRecognitionResult& result) override {
    return delegate_->AddTranscription(result);
  }

  void OnAudioStreamEnd(captions::CaptionBubbleContext*) override {
    delegate_->OnAudioStreamEnd();
  }

  void UpdateCaptionStyle(std::optional<ui::CaptionStyle> style) override {
    delegate_->AddStyleUpdate(style);
  }

  // Uninteresting methods
  void OnError(captions::CaptionBubbleContext*,
               captions::CaptionBubbleErrorType,
               captions::OnErrorClickedCallback,
               captions::OnDoNotShowAgainClickedCallback) override {}
  bool IsWidgetVisibleForTesting() override { return false; }
  bool IsGenericErrorMessageVisibleForTesting() override { return false; }
  std::string GetBubbleLabelTextForTesting() override { return std::string(); }
  void CloseActiveModelForTesting() override {}
  void OnLanguageIdentificationEvent(
      captions::CaptionBubbleContext*,
      const media::mojom::LanguageIdentificationEventPtr&) override {}

 private:
  raw_ptr<FakeCaptionControllerDelegate> delegate_;
};

}  // namespace

FakeCaptionControllerDelegate::FakeCaptionControllerDelegate() = default;
FakeCaptionControllerDelegate::~FakeCaptionControllerDelegate() = default;

std::unique_ptr<captions::CaptionBubbleController>
FakeCaptionControllerDelegate::CreateCaptionBubbleController(
    PrefService*,
    const std::string&) {
  caption_bubble_alive_ = true;
  ++create_bubble_controller_count_;
  return std::make_unique<FakeCaptionBubbleController>(this);
}

void FakeCaptionControllerDelegate::AddCaptionStyleObserver(
    ui::NativeThemeObserver* observer) {
  style_observer_ = observer;
}

void FakeCaptionControllerDelegate::RemoveCaptionStyleObserver(
    ui::NativeThemeObserver* observer) {
  style_observer_ = nullptr;
}

size_t FakeCaptionControllerDelegate::GetCreateBubbleControllerCount() {
  return create_bubble_controller_count_;
}

ui::NativeThemeObserver*
FakeCaptionControllerDelegate::GetCaptionStyleObserver() {
  return style_observer_;
}

size_t FakeCaptionControllerDelegate::GetOnAudioStreamEndCount() {
  return audio_stream_end_count_;
}

const std::vector<std::optional<ui::CaptionStyle>>&
FakeCaptionControllerDelegate::GetUpdateCaptionStyleUpdates() {
  return caption_style_updates_;
}

const std::vector<media::SpeechRecognitionResult>&
FakeCaptionControllerDelegate::GetTranscriptions() {
  return transcriptions_;
}

bool FakeCaptionControllerDelegate::AddTranscription(
    media::SpeechRecognitionResult transcription) {
  transcriptions_.push_back(std::move(transcription));
  return on_transcritption_success_;
}

void FakeCaptionControllerDelegate::SetOnTranscriptionSuccess(bool success) {
  on_transcritption_success_ = success;
}

void FakeCaptionControllerDelegate::OnAudioStreamEnd() {
  ++audio_stream_end_count_;
}

void FakeCaptionControllerDelegate::AddStyleUpdate(
    std::optional<ui::CaptionStyle> style) {
  caption_style_updates_.push_back(std::move(style));
}

bool FakeCaptionControllerDelegate::IsCaptionBubbleAlive() {
  return caption_bubble_alive_;
}

void FakeCaptionControllerDelegate::SetCaptionBubbleDestroyed() {
  caption_bubble_alive_ = false;
}

}  // namespace ash::babelorca
