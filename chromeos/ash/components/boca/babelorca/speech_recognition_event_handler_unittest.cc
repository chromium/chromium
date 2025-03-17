// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/speech_recognition_event_handler.h"

#include <string>

#include "base/test/bind.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {
constexpr char kDefaultLanguage[] = "en-US";
constexpr char kAlternativeLanguage[] = "de-DE";
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

  event_handler.SetTranscriptionResultCallback(
      base::BindLambdaForTesting(
          [&callback_invoked, &transcription, &language](
              const media::SpeechRecognitionResult& result,
              const std::string& source_language) {
            transcription = result.transcription;
            language = source_language;
            callback_invoked = true;
          }),
      base::DoNothing());

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

  event_handler.SetTranscriptionResultCallback(
      base::BindLambdaForTesting(
          [&callback_invoked](const media::SpeechRecognitionResult& result,
                              const std::string& source_language) {
            callback_invoked = true;
          }),
      base::DoNothing());

  event_handler.OnSpeechResult(std::nullopt);
  EXPECT_FALSE(callback_invoked);
}

// we should not invoke the callback if it has been unset.
TEST(SpeechRecognitionEventHandlerTest, WillNotInvokeWithEmptyCallback) {
  bool callback_invoked = false;
  SpeechRecognitionEventHandler event_handler(kDefaultLanguage);

  event_handler.SetTranscriptionResultCallback(
      base::BindLambdaForTesting(
          [&callback_invoked](const media::SpeechRecognitionResult& result,
                              const std::string& source_language) {
            callback_invoked = true;
          }),
      base::DoNothing());
  event_handler.RemoveSpeechRecognitionObservation();

  event_handler.OnSpeechResult(
      media::SpeechRecognitionResult(kTranscript, /*is_final=*/true));
  EXPECT_FALSE(callback_invoked);
}

// Tests that source language changes on LanguageChangeEvent
TEST(SpeechRecognitionEventHandlerTest,
     ChangesSourceLanguageOnLanguageIdentificationEvent) {
  size_t num_transcript_callback_invocations = 0u;
  bool language_id_callback_invoked = false;
  std::string on_recognition_source_language;
  std::string on_language_id_source_language;
  std::string transcript;
  media::mojom::AsrSwitchResult asr_switch_result;
  SpeechRecognitionEventHandler event_handler(kDefaultLanguage);
  media::mojom::LanguageIdentificationEventPtr language_event =
      media::mojom::LanguageIdentificationEvent::New(
          kAlternativeLanguage, media::mojom::ConfidenceLevel::kConfident);
  language_event->asr_switch_result =
      media::mojom::AsrSwitchResult::kSwitchSucceeded;

  event_handler.SetTranscriptionResultCallback(
      base::BindLambdaForTesting(
          [&num_transcript_callback_invocations,
           &on_recognition_source_language,
           &transcript](const media::SpeechRecognitionResult& result,
                        const std::string& source_language) {
            transcript = result.transcription;
            on_recognition_source_language = source_language;
            ++num_transcript_callback_invocations;
          }),
      base::BindLambdaForTesting(
          [&language_id_callback_invoked, &asr_switch_result,
           &on_language_id_source_language](
              const media::mojom::LanguageIdentificationEventPtr& event) {
            ASSERT_TRUE(event->asr_switch_result.has_value());
            asr_switch_result = event->asr_switch_result.value();
            on_language_id_source_language = event->language;
            language_id_callback_invoked = true;
          }));

  // Verify default behavior before switching language.
  event_handler.OnSpeechResult(
      media::SpeechRecognitionResult(kTranscript, /*is_final=*/true));
  ASSERT_EQ(num_transcript_callback_invocations, 1u);
  EXPECT_EQ(on_recognition_source_language, kDefaultLanguage);
  EXPECT_EQ(transcript, kTranscript);

  // Simulate id event, ensure that source language changes.
  event_handler.OnLanguageIdentificationEvent(std::move(language_event));
  ASSERT_TRUE(language_id_callback_invoked);
  EXPECT_EQ(asr_switch_result, media::mojom::AsrSwitchResult::kSwitchSucceeded);
  EXPECT_EQ(on_language_id_source_language, kAlternativeLanguage);

  event_handler.OnSpeechResult(
      media::SpeechRecognitionResult(kTranscript, /*is_final=*/true));
  EXPECT_EQ(num_transcript_callback_invocations, 2u);
  EXPECT_EQ(on_recognition_source_language, kAlternativeLanguage);
  EXPECT_EQ(transcript, kTranscript);
}

}  // namespace ash::babelorca
