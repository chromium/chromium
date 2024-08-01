// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/soda_speech_recognizer_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition_error_code.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

class SodaSpeechRecognizerImplTest
    : public media::mojom::SpeechRecognitionRecognizer,
      public media::mojom::SpeechRecognitionSessionClient,
      public testing::TestWithParam<bool> {
 public:
  SodaSpeechRecognizerImplTest() {
    recognizer_ = std::make_unique<SodaSpeechRecognizerImpl>(
        /*continuous=*/GetParam(), /*sample_rate=*/48000,
        speech_recognition_recognizer_.BindNewPipeAndPassRemote(),
        mojo::NullReceiver(), session_client_.BindNewPipeAndPassRemote(),
        mojo::NullReceiver());
  }

  ~SodaSpeechRecognizerImplTest() override = default;

  void CheckEventsConsistency() {
    // Note: "!x || y" == "x implies y".
    EXPECT_TRUE(!recognition_ended_ || recognition_started_);
    EXPECT_TRUE(!audio_ended_ || audio_started_);
    EXPECT_TRUE(!sound_ended_ || sound_started_);
    EXPECT_TRUE(!audio_started_ || recognition_started_);
    EXPECT_TRUE(!sound_started_ || audio_started_);
    EXPECT_TRUE(!audio_ended_ || (sound_ended_ || !sound_started_));
    EXPECT_TRUE(!recognition_ended_ || (audio_ended_ || !audio_started_));
  }

  void CheckFinalEventsConsistency() {
    // Note: "!(x ^ y)" == "(x && y) || (!x && !x)".
    EXPECT_FALSE(recognition_started_ ^ recognition_ended_);
    EXPECT_FALSE(audio_started_ ^ audio_ended_);
    EXPECT_FALSE(sound_started_ ^ sound_ended_);
  }

  // media::mojom::SpeechRecognitionRecognizer implementation.
  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer) override {}
  void OnLanguageChanged(const std::string& language) override {}
  void OnMaskOffensiveWordsChanged(bool mask_offensive_words) override {}
  void MarkDone() override {}

  // media::mojom::SpeechRecognitionSessionClient implementation.
  void ResultRetrieved(std::vector<media::mojom::WebSpeechRecognitionResultPtr>
                           results) override {
    result_received_ = true;
  }

  void ErrorOccurred(media::mojom::SpeechRecognitionErrorPtr error) override {
    EXPECT_TRUE(recognition_started_);
    EXPECT_FALSE(recognition_ended_);
    error_ = error->code;
  }

  void Started() override {
    recognition_started_ = true;
    CheckEventsConsistency();
  }

  void AudioStarted() override {
    audio_started_ = true;
    CheckEventsConsistency();
  }

  void SoundStarted() override {
    sound_started_ = true;
    CheckEventsConsistency();
  }

  void SoundEnded() override {
    sound_ended_ = true;
    CheckEventsConsistency();
  }

  void AudioEnded() override {
    audio_ended_ = true;
    CheckEventsConsistency();
  }

  void Ended() override {
    recognition_ended_ = true;
    CheckEventsConsistency();
  }

  void OnSpeechRecognitionRecognitionEvent() {
    recognizer_->OnSpeechRecognitionRecognitionEvent(
        media::SpeechRecognitionResult(
            "Quokkas are known as the happiest animals in the world due to "
            "their seemingly constant smiles and friendly demeanor.",
            /*is_final=*/true),
        base::BindOnce([](bool continue_recognition) {}));
  }

  void OnSpeechRecognitionError() { recognizer_->OnSpeechRecognitionError(); }

  void AddAudio() {
    recognizer_->AddAudioFromRenderer(media::mojom::AudioDataS16::New());
  }

  void Abort() { recognizer_->Abort(); }
  void StopCapture() { recognizer_->StopCapture(); }

 protected:
  base::test::TaskEnvironment environment_;
  mojo::Receiver<media::mojom::SpeechRecognitionSessionClient> session_client_{
      this};
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizer>
      speech_recognition_recognizer_{this};

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<SodaSpeechRecognizerImpl> recognizer_;
  bool recognition_started_ = false;
  bool recognition_ended_ = false;
  bool result_received_ = false;
  bool audio_started_ = false;
  bool audio_ended_ = false;
  bool sound_started_ = false;
  bool sound_ended_ = false;
  media::mojom::SpeechRecognitionErrorCode error_ =
      media::mojom::SpeechRecognitionErrorCode::kNone;
};

TEST_P(SodaSpeechRecognizerImplTest, Start) {
  // Recognition is started automatically as soon as the recognizer is created.
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.
  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  CheckEventsConsistency();
}

TEST_P(SodaSpeechRecognizerImplTest, RecognitionEvent) {
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.
  EXPECT_TRUE(recognition_started_);

  AddAudio();
  base::RunLoop().RunUntilIdle();  // EVENT_AUDIO_DATA processing.
  EXPECT_TRUE(audio_started_);
  CheckEventsConsistency();

  OnSpeechRecognitionRecognitionEvent();
  base::RunLoop().RunUntilIdle();  // EVENT_ENGINE_RESULT processing.
  EXPECT_TRUE(result_received_);
  CheckEventsConsistency();

  StopCapture();
  base::RunLoop().RunUntilIdle();  // EVENT_STOP_CAPTURE processing.
  EXPECT_TRUE(recognition_ended_);
  CheckEventsConsistency();
  CheckFinalEventsConsistency();
}

TEST_P(SodaSpeechRecognizerImplTest, Abort) {
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.
  EXPECT_TRUE(recognition_started_);

  Abort();
  base::RunLoop().RunUntilIdle();  // EVENT_ABORT processing.
  EXPECT_TRUE(recognition_ended_);
  CheckEventsConsistency();
  CheckFinalEventsConsistency();
}

TEST_P(SodaSpeechRecognizerImplTest, EngineError) {
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.
  EXPECT_TRUE(recognition_started_);

  OnSpeechRecognitionError();
  base::RunLoop().RunUntilIdle();  // EVENT_ENGINE_ERROR processing.
  EXPECT_TRUE(recognition_ended_);
  CheckEventsConsistency();
  CheckFinalEventsConsistency();
}

INSTANTIATE_TEST_SUITE_P(All, SodaSpeechRecognizerImplTest, testing::Bool());

}  // namespace speech
