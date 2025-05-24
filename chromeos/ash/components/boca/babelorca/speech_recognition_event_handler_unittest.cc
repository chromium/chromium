// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/speech_recognition_event_handler.h"

#include <string>

#include "base/test/bind.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {
constexpr char kDefaultLanguage[] = "en-US";
constexpr char kAlternativeLanguage[] = "de-DE";
constexpr char kTranscript[] = "hello there.";

class MockSpeechRecognizerObserver
    : public BabelOrcaSpeechRecognizer::Observer {
 public:
  MockSpeechRecognizerObserver() = default;
  ~MockSpeechRecognizerObserver() override = default;

  MOCK_METHOD(void,
              OnTranscriptionResult,
              (const media::SpeechRecognitionResult& result,
               const std::string& source_language),
              (override));
  MOCK_METHOD(void,
              OnLanguageIdentificationEvent,
              (const media::mojom::LanguageIdentificationEventPtr& event),
              (override));
};
}  // namespace

// Tests that the callback passed to the event handler will be fired
// if the preconditions are met.  Namely that the callback itself is set
// and that the result passed to `OnSpeechResult` has a value.
TEST(SpeechRecognitionEventHandlerTest, WillEmitEventWithPreConditions) {
  MockSpeechRecognizerObserver mock_observer;
  SpeechRecognitionEventHandler event_handler(kDefaultLanguage);
  event_handler.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer,
              OnTranscriptionResult(media::SpeechRecognitionResult(
                                        kTranscript, /*is_final=*/true),
                                    kDefaultLanguage))
      .Times(1);

  event_handler.OnSpeechResult(
      media::SpeechRecognitionResult(kTranscript, /*is_final=*/true));
}

// We should not invoke the callback if there is an empty result.
TEST(SpeechRecognitionEventHandlerTest, WillNotEmitEventWithEmptyResult) {
  MockSpeechRecognizerObserver mock_observer;
  SpeechRecognitionEventHandler event_handler(kDefaultLanguage);
  event_handler.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnTranscriptionResult).Times(0);

  event_handler.OnSpeechResult(std::nullopt);
}

// we should not invoke the callback if it has been unset.
TEST(SpeechRecognitionEventHandlerTest, WillNotEmitEventAfterRemovingObserver) {
  MockSpeechRecognizerObserver mock_observer;
  SpeechRecognitionEventHandler event_handler(kDefaultLanguage);
  event_handler.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnTranscriptionResult).Times(0);

  event_handler.RemoveObserver(&mock_observer);
  event_handler.OnSpeechResult(
      media::SpeechRecognitionResult(kTranscript, /*is_final=*/true));
}

// Tests that source language changes on LanguageChangeEvent
TEST(SpeechRecognitionEventHandlerTest,
     ChangesSourceLanguageOnLanguageIdentificationEvent) {
  MockSpeechRecognizerObserver mock_observer;
  SpeechRecognitionEventHandler event_handler(kDefaultLanguage);
  media::mojom::LanguageIdentificationEventPtr language_event =
      media::mojom::LanguageIdentificationEvent::New(
          kAlternativeLanguage, media::mojom::ConfidenceLevel::kConfident);
  language_event->asr_switch_result =
      media::mojom::AsrSwitchResult::kSwitchSucceeded;

  EXPECT_CALL(mock_observer,
              OnTranscriptionResult(media::SpeechRecognitionResult(
                                        kTranscript, /*is_final=*/true),
                                    kDefaultLanguage))
      .Times(1);
  EXPECT_CALL(mock_observer,
              OnTranscriptionResult(media::SpeechRecognitionResult(
                                        kTranscript, /*is_final=*/true),
                                    kAlternativeLanguage))
      .Times(1);
  // Cannot directly match two of these mojom pointers so we have to decompose
  // it and compare its members in WillOnce.
  EXPECT_CALL(mock_observer, OnLanguageIdentificationEvent)
      .WillOnce([](const media::mojom::LanguageIdentificationEventPtr& result) {
        EXPECT_EQ(result->asr_switch_result,
                  media::mojom::AsrSwitchResult::kSwitchSucceeded);
        EXPECT_EQ(result->language, kAlternativeLanguage);
      });

  event_handler.AddObserver(&mock_observer);
  // Verify default behavior before switching language.
  event_handler.OnSpeechResult(
      media::SpeechRecognitionResult(kTranscript, /*is_final=*/true));

  // Simulate id event, ensure that source language changes.
  event_handler.OnLanguageIdentificationEvent(std::move(language_event));
  event_handler.OnSpeechResult(
      media::SpeechRecognitionResult(kTranscript, /*is_final=*/true));
}

}  // namespace ash::babelorca
