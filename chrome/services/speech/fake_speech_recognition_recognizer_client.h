// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_FAKE_SPEECH_RECOGNITION_RECOGNIZER_CLIENT_H_
#define CHROME_SERVICES_SPEECH_FAKE_SPEECH_RECOGNITION_RECOGNIZER_CLIENT_H_

#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace speech {

class FakeSpeechRecognitionRecognizerClient
    : public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override {}
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {}
  void OnSpeechRecognitionError() override {}
  void OnSpeechRecognitionStopped() override {}
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_FAKE_SPEECH_RECOGNITION_RECOGNIZER_CLIENT_H_
