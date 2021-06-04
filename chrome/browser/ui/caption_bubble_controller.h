// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CAPTION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_CAPTION_BUBBLE_CONTROLLER_H_

#include <memory>
#include <string>

#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/native_theme/caption_style.h"

namespace captions {

class LiveCaptionSpeechRecognitionHost;

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
  CaptionBubbleController() = default;
  virtual ~CaptionBubbleController() = default;
  CaptionBubbleController(const CaptionBubbleController&) = delete;
  CaptionBubbleController& operator=(const CaptionBubbleController&) = delete;

  static std::unique_ptr<CaptionBubbleController> Create();

  // Called when a transcription is received from the service. Returns whether
  // the transcription result was set on the caption bubble successfully.
  // Transcriptions will halt if this returns false.
  virtual bool OnTranscription(
      LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host,
      const media::SpeechRecognitionResult& result) = 0;

  // Called when the speech service has an error.
  virtual void OnError(LiveCaptionSpeechRecognitionHost*
                           live_caption_speech_recognition_host) = 0;

  // Called when the audio stream has ended.
  virtual void OnAudioStreamEnd(LiveCaptionSpeechRecognitionHost*
                                    live_caption_speech_recognition_host) = 0;

  // Called when the caption style changes.
  virtual void UpdateCaptionStyle(
      absl::optional<ui::CaptionStyle> caption_style) = 0;

 private:
  friend class LiveCaptionControllerTest;
  friend class LiveCaptionSpeechRecognitionHostTest;

  virtual bool IsWidgetVisibleForTesting() = 0;
  virtual std::string GetBubbleLabelTextForTesting() = 0;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_CAPTION_BUBBLE_CONTROLLER_H_
