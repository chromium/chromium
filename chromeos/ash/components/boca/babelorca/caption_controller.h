// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CAPTION_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CAPTION_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/live_caption/caption_controller_base.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

class PrefService;

namespace captions {
class CaptionBubbleContext;
class CaptionBubbleController;
class CaptionBubbleSettings;
class TranslationViewWrapperBase;
}  // namespace captions

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ash::babelorca {

class CaptionBubbleSettingsImpl;

class CaptionController : public ::captions::CaptionControllerBase {
 public:
  CaptionController(
      std::unique_ptr<::captions::CaptionBubbleContext> caption_bubble_context,
      PrefService* profile_prefs,
      const std::string& application_locale,
      std::unique_ptr<CaptionBubbleSettingsImpl> caption_bubble_settings,
      std::unique_ptr<Delegate> delegate = nullptr);

  CaptionController(const CaptionController&) = delete;
  CaptionController& operator=(const CaptionController&) = delete;

  ~CaptionController() override;

  // Routes a transcription to the CaptionBubbleController. Returns whether the
  // transcription result was routed successfully.
  bool DispatchTranscription(const media::SpeechRecognitionResult& result);

  void OnLanguageIdentificationEvent(
      const media::mojom::LanguageIdentificationEventPtr& event);

  void StartLiveCaption();

  void StopLiveCaption();

  void SetLiveTranslateEnabled(bool enabled);

  void SetTranslateAllowed(bool allowed);

  bool IsTranslateAllowedAndEnabled();

  std::string GetLiveTranslateTargetLanguageCode();

 private:
  // CaptionControllerBase:
  captions::CaptionBubbleSettings* caption_bubble_settings() override;
  std::unique_ptr<captions::TranslationViewWrapperBase>
  CreateTranslationViewWrapper() override;

  std::unique_ptr<::captions::CaptionBubbleContext> caption_bubble_context_;
  const std::unique_ptr<CaptionBubbleSettingsImpl> caption_bubble_settings_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CAPTION_CONTROLLER_H_
