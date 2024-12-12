// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/speech_recognition_event_handler.h"

#include <string>

#include "base/test/bind.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {
constexpr char kDefaultLanguage[] = "en-US";
constexpr char kTranscript[] = "hello there.";
}  // namespace

// Tests that the callback passed to the event handler will be fired
// if the preconditions are met.  Namely that the callback itself is set
// and that the result passed to `OnSpeechResult` has a value.
TEST(SpeechRecognitionEventHandlerTest, WillInvokeCallbackWithPreConditions) {
  bool callback_invoked = false;
  std::string transcription;
  std::string language;
  SpeechRecognitionEventHandler event_handler(kDefaultLanguage);

  event_handler.SetTranscriptionResultCallback(base::BindLambdaForTesting(
      [&callback_invoked, &transcription, &language](
          const media::SpeechRecognitionResult& result,
          const std::string& source_language) {
        transcription = result.transcription;
        language = source_language;
        callback_invoked = true;
      }));

  event_handler.OnSpeechResult(
      media::SpeechRecognitionResult(kTranscript, /*is_final=*/true));
  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(transcription, kTranscript);
  EXPECT_EQ(language, kDefaultLanguage);
}

// We should not invoke the callback if there is an empty result.
TEST(SpeechRecognitionEventHandlerTest, WillNotInvokeWithEmptyResult) {
  bool callback_invoked = false;
  SpeechRecognitionEventHandler event_handler(kDefaultLanguage);

  event_handler.SetTranscriptionResultCallback(base::BindLambdaForTesting(
      [&callback_invoked](const media::SpeechRecognitionResult& result,
                          const std::string& source_language) {
        callback_invoked = true;
      }));

  event_handler.OnSpeechResult(std::nullopt);
  EXPECT_FALSE(callback_invoked);
}

// we should not invoke the callback if it has been unset.
TEST(SpeechRecognitionEventHandlerTest, WillNotInvokeWithEmptyCallback) {
  bool callback_invoked = false;
  SpeechRecognitionEventHandler event_handler(kDefaultLanguage);

  event_handler.SetTranscriptionResultCallback(base::BindLambdaForTesting(
      [&callback_invoked](const media::SpeechRecognitionResult& result,
                          const std::string& source_language) {
        callback_invoked = true;
      }));
  event_handler.RemoveTranscriptionResultObservation();

  event_handler.OnSpeechResult(
      media::SpeechRecognitionResult(kTranscript, /*is_final=*/true));
  EXPECT_FALSE(callback_invoked);
}

}  // namespace ash::babelorca
