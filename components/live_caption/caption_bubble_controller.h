// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTROLLER_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "components/live_caption/views/caption_bubble.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "ui/native_theme/caption_style.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace captions {

class CaptionBubbleContext;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Controller
//
//  The interface for the caption bubble controller. It controls the caption
//  bubble. It is responsible for tasks such as post-processing of the text that
//  might need to occur before it is displayed in the bubble, and hiding or
//  showing the caption bubble when captions are received. There exists one
//  caption bubble controller per profile.
//
class CaptionBubbleController {
 public:
  explicit CaptionBubbleController() = default;
  virtual ~CaptionBubbleController() = default;
  CaptionBubbleController(const CaptionBubbleController&) = delete;
  CaptionBubbleController& operator=(const CaptionBubbleController&) = delete;

  static std::unique_ptr<CaptionBubbleController> Create(
      PrefService* profile_prefs,
      const std::string& application_locale);

  // Called when a transcription is received from the service. Returns whether
  // the transcription result was set on the caption bubble successfully.
  // Transcriptions will halt if this returns false.
  virtual bool OnTranscription(
      CaptionBubbleContext* caption_bubble_context,
      const media::SpeechRecognitionResult& result) = 0;

  // Called when the speech service has an error.
  virtual void OnError(
      CaptionBubbleContext* caption_bubble_context,
      CaptionBubbleErrorType error_type,
      OnErrorClickedCallback error_clicked_callback,
      OnDoNotShowAgainClickedCallback error_silenced_callback) = 0;

  // Called when the audio stream has ended.
  virtual void OnAudioStreamEnd(
      CaptionBubbleContext* caption_bubble_context) = 0;

  // Called when the caption style changes.
  virtual void UpdateCaptionStyle(
      std::optional<ui::CaptionStyle> caption_style) = 0;

  virtual bool IsWidgetVisibleForTesting() = 0;
  virtual bool IsGenericErrorMessageVisibleForTesting() = 0;
  virtual std::string GetBubbleLabelTextForTesting() = 0;
  virtual void CloseActiveModelForTesting() = 0;

  virtual void OnLanguageIdentificationEvent(
      CaptionBubbleContext* caption_bubble_context,
      const media::mojom::LanguageIdentificationEventPtr& event) = 0;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTROLLER_H_
