// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/soda_speech_recognizer_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
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
    EXPECT_TRUE(!recognition_context_updated_ || recognition_started_);
    EXPECT_TRUE(!recognition_context_updated_ || recognition_started_);
  }

  void CheckFinalEventsConsistency() {
    // Note: "!(x ^ y)" == "(x && y) || (!x && !x)".
    EXPECT_FALSE(recognition_started_ ^ recognition_ended_);
    EXPECT_FALSE(audio_started_ ^ audio_ended_);
    EXPECT_FALSE(sound_started_ ^ sound_ended_);
  }

  // media::mojom::SpeechRecognitionRecognizer implementation.
  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer,
      std::optional<base::TimeDelta>) override {}
  void OnLanguageChanged(const std::string& language) override {}
  void OnMaskOffensiveWordsChanged(bool mask_offensive_words) override {}
  void MarkDone() override {
    mark_done_called_ = true;
    MaybeQuit();
  }
  void UpdateRecognitionContext(
      const media::SpeechRecognitionRecognitionContext& recognition_context)
      override {
    recognition_context_updated_ = true;
    MaybeQuit();
  }

  // media::mojom::SpeechRecognitionSessionClient implementation.
  void ResultRetrieved(std::vector<media::mojom::WebSpeechRecognitionResultPtr>
                           results) override {
    result_received_ = true;
    last_results_ = std::move(results);
    MaybeQuit();
  }

  void ErrorOccurred(media::mojom::SpeechRecognitionErrorPtr error) override {
    EXPECT_TRUE(recognition_started_);
    EXPECT_FALSE(recognition_ended_);
    error_ = error->code;
    MaybeQuit();
  }

  void Started() override {
    recognition_started_ = true;
    CheckEventsConsistency();
    MaybeQuit();
  }

  void AudioStarted() override {
    audio_started_ = true;
    CheckEventsConsistency();
    MaybeQuit();
  }

  void SoundStarted() override {
    sound_started_ = true;
    CheckEventsConsistency();
    MaybeQuit();
  }

  void SoundEnded() override {
    sound_ended_ = true;
    CheckEventsConsistency();
    MaybeQuit();
  }

  void AudioEnded() override {
    audio_ended_ = true;
    CheckEventsConsistency();
    MaybeQuit();
  }

  void Ended() override {
    recognition_ended_ = true;
    CheckEventsConsistency();
    MaybeQuit();
  }

  void OnSpeechRecognitionRecognitionEvent() {
    recognizer_->OnSpeechRecognitionRecognitionEvent(
        media::SpeechRecognitionResult(
            "Quokkas are known as the happiest animals in the world due to "
            "their seemingly constant smiles and friendly demeanor.",
            /*is_final=*/true),
        base::BindOnce([](bool continue_recognition) {}));
  }

  void OnSpeechRecognitionRecognitionEventPartial() {
    recognizer_->OnSpeechRecognitionRecognitionEvent(
        media::SpeechRecognitionResult("Quokkas", /*is_final=*/false),
        base::BindOnce([](bool) {}));
  }

  void OnSpeechRecognitionRecognitionEventWithTiming() {
    media::SpeechRecognitionResult speech_result(
        "Quokkas are known as the happiest animals in the world",
        /*is_final=*/true);
    speech_result.timing_information = media::TimingInformation();
    speech_result.timing_information->audio_start_time =
        base::Milliseconds(100);
    speech_result.timing_information->audio_end_time = base::Milliseconds(600);
    recognizer_->OnSpeechRecognitionRecognitionEvent(
        speech_result, base::BindOnce([](bool continue_recognition) {}));
  }

  void OnSpeechRecognitionError() { recognizer_->OnSpeechRecognitionError(); }

  void AddAudio(int frame_count = 4800, bool loud = false) {
    auto audio_data = media::mojom::AudioDataS16::New();
    audio_data->channel_count = 1;
    audio_data->sample_rate = 48000;
    audio_data->frame_count = frame_count;
    audio_data->data.resize(frame_count, 0);
    if (loud) {
      for (int i = 0; i < frame_count; ++i) {
        // High amplitude to trigger the endpointer's speech detection.
        audio_data->data[i] = 32767;
      }
    }
    recognizer_->AddAudioFromRenderer(std::move(audio_data));
  }

  void Abort() { recognizer_->Abort(); }
  void StopCapture() { recognizer_->StopCapture(); }
  void RecognizerUpdateRecognitionContext(
      const media::SpeechRecognitionRecognitionContext& recognition_context) {
    recognizer_->UpdateRecognitionContext(recognition_context);
  }

  void WaitForCondition(base::FunctionRef<bool()> condition) {
    while (!condition()) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  void MaybeQuit() {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 protected:
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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
  bool recognition_context_updated_ = false;
  bool mark_done_called_ = false;
  std::vector<media::mojom::WebSpeechRecognitionResultPtr> last_results_;
  media::mojom::SpeechRecognitionErrorCode error_ =
      media::mojom::SpeechRecognitionErrorCode::kNone;
  base::OnceClosure quit_closure_;
};

TEST_P(SodaSpeechRecognizerImplTest, Start) {
  // Recognition is started automatically as soon as the recognizer is created.
  WaitForCondition([&]() { return recognition_started_; });
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  CheckEventsConsistency();
}

TEST_P(SodaSpeechRecognizerImplTest, RecognitionEvent) {
  WaitForCondition([&]() { return recognition_started_; });

  AddAudio();
  WaitForCondition([&]() { return audio_started_; });
  CheckEventsConsistency();

  OnSpeechRecognitionRecognitionEvent();
  WaitForCondition([&]() { return result_received_; });
  CheckEventsConsistency();

  StopCapture();
  WaitForCondition([&]() { return recognition_ended_; });
  CheckEventsConsistency();
  CheckFinalEventsConsistency();
}

TEST_P(SodaSpeechRecognizerImplTest, RecognitionEventWithTimingInformation) {
  WaitForCondition([&]() { return recognition_started_; });

  AddAudio();
  WaitForCondition([&]() { return audio_started_; });
  CheckEventsConsistency();

  OnSpeechRecognitionRecognitionEventWithTiming();
  WaitForCondition([&]() { return result_received_; });
  ASSERT_FALSE(last_results_.empty());
  EXPECT_EQ(last_results_[0]->audio_start_time, base::Milliseconds(100));
  EXPECT_EQ(last_results_[0]->audio_end_time, base::Milliseconds(600));

  StopCapture();
  WaitForCondition([&]() { return recognition_ended_; });
  CheckEventsConsistency();
  CheckFinalEventsConsistency();
}

TEST_P(SodaSpeechRecognizerImplTest, Abort) {
  WaitForCondition([&]() { return recognition_started_; });

  Abort();
  WaitForCondition([&]() { return recognition_ended_; });
  CheckEventsConsistency();
  CheckFinalEventsConsistency();
}

TEST_P(SodaSpeechRecognizerImplTest, EngineError) {
  WaitForCondition([&]() { return recognition_started_; });

  OnSpeechRecognitionError();
  WaitForCondition([&]() { return recognition_ended_; });
  CheckEventsConsistency();
  CheckFinalEventsConsistency();
}

TEST_P(SodaSpeechRecognizerImplTest, StopCaptureWithNoFinalResult) {
  WaitForCondition([&]() { return recognition_started_; });

  AddAudio();
  WaitForCondition([&]() { return audio_started_; });
  CheckEventsConsistency();

  OnSpeechRecognitionRecognitionEventPartial();
  WaitForCondition([&]() { return result_received_; });
  CheckEventsConsistency();

  StopCapture();
  WaitForCondition([&]() { return audio_ended_; });
  EXPECT_FALSE(recognition_ended_);
  environment_.FastForwardBy(base::Seconds(4));
  EXPECT_FALSE(recognition_ended_);
  environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(recognition_ended_);
}

TEST_P(SodaSpeechRecognizerImplTest, UpdateRecognitionContext) {
  // EVENT_START processing.
  WaitForCondition([&]() { return recognition_started_; });

  // EVENT_UPDATE_RECOGNITION_CONTEXT processing.
  RecognizerUpdateRecognitionContext(
      media::SpeechRecognitionRecognitionContext());
  WaitForCondition([&]() { return recognition_context_updated_; });
  CheckEventsConsistency();
}

TEST_P(SodaSpeechRecognizerImplTest, NoSpeechTimeoutBehavior) {
  WaitForCondition([&]() { return recognition_started_; });

  AddAudio();
  WaitForCondition([&]() { return audio_started_; });

  // Pass environment estimation.
  for (int i = 0; i < 30; ++i) {
    environment_.FastForwardBy(base::Milliseconds(100));
    AddAudio(4800, /*loud=*/false);
  }

  // Send 10 seconds of silence to trigger no-speech timeout.
  for (int i = 0; i < 100; ++i) {
    environment_.FastForwardBy(base::Milliseconds(100));
    AddAudio(4800, /*loud=*/false);
  }

  if (GetParam()) {
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(recognition_ended_);
    EXPECT_EQ(error_, media::mojom::SpeechRecognitionErrorCode::kNone);
  } else {
    WaitForCondition([&]() { return recognition_ended_; });
    EXPECT_TRUE(recognition_ended_);
    EXPECT_EQ(error_, media::mojom::SpeechRecognitionErrorCode::kNoSpeech);
  }
}

TEST_P(SodaSpeechRecognizerImplTest, FinalResultBehavior) {
  WaitForCondition([&]() { return recognition_started_; });

  AddAudio();
  WaitForCondition([&]() { return audio_started_; });

  // Go to recognizing
  OnSpeechRecognitionRecognitionEventPartial();
  WaitForCondition([&]() { return sound_started_; });

  EXPECT_FALSE(mark_done_called_);

  // Send final result
  OnSpeechRecognitionRecognitionEvent();

  if (GetParam()) {
    // continuous=true: should NOT stop capture automatically
    WaitForCondition([&]() { return result_received_; });
    EXPECT_FALSE(recognition_ended_);
    EXPECT_FALSE(mark_done_called_);
    // We need to stop it manually to end test clean
    StopCapture();
    WaitForCondition([&]() { return recognition_ended_; });
    EXPECT_TRUE(mark_done_called_);
  } else {
    // continuous=false: should stop capture automatically and end immediately
    WaitForCondition([&]() { return recognition_ended_; });
    EXPECT_TRUE(mark_done_called_);
  }
}

TEST_P(SodaSpeechRecognizerImplTest, StopCaptureThenReceiveFinalResult) {
  WaitForCondition([&]() { return recognition_started_; });

  AddAudio();
  WaitForCondition([&]() { return audio_started_; });

  // Go to recognizing
  OnSpeechRecognitionRecognitionEventPartial();
  WaitForCondition([&]() { return sound_started_; });

  StopCapture();
  WaitForCondition([&]() { return audio_ended_; });
  EXPECT_FALSE(recognition_ended_);  // waiting for final result
  EXPECT_TRUE(mark_done_called_);

  // Send final result
  OnSpeechRecognitionRecognitionEvent();
  WaitForCondition([&]() { return recognition_ended_; });
}

TEST_P(SodaSpeechRecognizerImplTest, MarkDoneCalledOnStop) {
  WaitForCondition([&]() { return recognition_started_; });

  AddAudio();
  WaitForCondition([&]() { return audio_started_; });

  EXPECT_FALSE(mark_done_called_);

  StopCapture();
  WaitForCondition([&]() { return audio_ended_; });

  EXPECT_TRUE(mark_done_called_);
}

INSTANTIATE_TEST_SUITE_P(All, SodaSpeechRecognizerImplTest, testing::Bool());

}  // namespace speech
