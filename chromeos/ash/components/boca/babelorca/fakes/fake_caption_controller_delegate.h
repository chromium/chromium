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
#include "media/mojo/mojom/speech_recognition.mojom-forward.h"

namespace captions {
class CaptionBubbleController;
class CaptionBubbleSettings;
class TranslationViewWrapperBase;
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
  CreateCaptionBubbleController(
      captions::CaptionBubbleSettings*,
      const std::string&,
      std::unique_ptr<captions::TranslationViewWrapperBase>) override;

  void AddCaptionStyleObserver(ui::NativeThemeObserver* observer) override;

  void RemoveCaptionStyleObserver(ui::NativeThemeObserver* observer) override;

  size_t GetCreateBubbleControllerCount();

  ui::NativeThemeObserver* GetCaptionStyleObserver();

  size_t GetOnLanguageIdentificationEventCount();

  const std::vector<std::optional<ui::CaptionStyle>>&
  GetUpdateCaptionStyleUpdates();

  const std::vector<media::SpeechRecognitionResult>& GetTranscriptions();

  const std::vector<media::mojom::LanguageIdentificationEventPtr>&
  GetLanguageIdentificationEvents();

  bool AddTranscription(media::SpeechRecognitionResult transcription);

  void SetOnTranscriptionSuccess(bool success);

  void OnLanguageIdentificationEvent(
      const media::mojom::LanguageIdentificationEventPtr& event);

  void AddStyleUpdate(std::optional<ui::CaptionStyle> style);

  bool IsCaptionBubbleAlive();

  void SetCaptionBubbleDestroyed();

 private:
  bool caption_bubble_alive_ = false;
  bool on_transcritption_success_ = true;
  size_t create_bubble_controller_count_ = 0;
  std::vector<media::mojom::LanguageIdentificationEventPtr>
      language_identification_events_;
  std::vector<std::optional<ui::CaptionStyle>> caption_style_updates_;
  std::vector<media::SpeechRecognitionResult> transcriptions_;
  raw_ptr<ui::NativeThemeObserver> style_observer_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_FAKES_FAKE_CAPTION_CONTROLLER_DELEGATE_H_
