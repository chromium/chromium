// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_recognizer_impl.h"

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "chrome/services/speech/soda/mock_soda_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;

namespace speech {

namespace {

constexpr char kPrimaryLanguageName[] = "en-US";

}  // namespace

class SpeechRecognitionRecognizerImplTest
    : public testing::Test,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  SpeechRecognitionRecognizerImplTest() = default;
  SpeechRecognitionRecognizerImplTest(
      const SpeechRecognitionRecognizerImplTest&) = delete;
  SpeechRecognitionRecognizerImplTest& operator=(
      const SpeechRecognitionRecognizerImplTest&) = delete;
  ~SpeechRecognitionRecognizerImplTest() override = default;

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override {
    last_received_result_ = result;
    std::move(reply).Run(true);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
  void OnSpeechRecognitionStopped() override {}
  void OnSpeechRecognitionError() override {}
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {}

  void CreateRecognizer(media::mojom::SpeechRecognitionOptionsPtr options) {
    recognizer_ = std::make_unique<SpeechRecognitionRecognizerImpl>(
        receiver_.BindNewPipeAndPassRemote(), std::move(options),
        base::FilePath(), config_paths_, kPrimaryLanguageName,
        /*mask_offensive_words=*/true);
    auto soda_client = std::make_unique<NiceMock<::soda::MockSodaClient>>();
    soda_client_ = soda_client.get();
    recognizer_->SetSodaClientForTesting(std::move(soda_client));
  }

  media::mojom::SpeechRecognitionOptionsPtr CreateOptions(
      std::optional<media::SpeechRecognitionRecognitionContext>
          recognition_context = std::nullopt) {
    media::mojom::SpeechRecognitionOptionsPtr options =
        media::mojom::SpeechRecognitionOptions::New();
    options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
    options->enable_formatting = false;
    options->recognizer_client_type =
        media::mojom::RecognizerClientType::kLiveCaption;
    options->skip_continuously_empty_audio = false;
    if (recognition_context) {
      options->recognition_context = std::move(*recognition_context);
    }
    return options;
  }

  base::flat_map<std::string, base::FilePath> config_paths() {
    return config_paths_;
  }

 protected:
  void WaitForRecognitionEvent() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  // Sends a dummy audio buffer to the recognizer to populate the timestamp
  // estimator.
  void SendAudio(SpeechRecognitionRecognizerImpl* recognizer,
                 base::TimeDelta duration,
                 base::TimeDelta media_start_pts) {
    auto audio_buffer = media::mojom::AudioDataS16::New();
    audio_buffer->sample_rate = 16000;
    audio_buffer->channel_count = 1;
    audio_buffer->frame_count = duration.InSeconds() * 16000;
    audio_buffer->data.resize(audio_buffer->frame_count, 0);
    recognizer->SendAudioToSpeechRecognitionService(std::move(audio_buffer),
                                                    media_start_pts);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient> receiver_{
      this};
  base::flat_map<std::string, base::FilePath> config_paths_;
  media::SpeechRecognitionResult last_received_result_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<SpeechRecognitionRecognizerImpl> recognizer_;
  raw_ptr<NiceMock<::soda::MockSodaClient>> soda_client_ = nullptr;
};

TEST_F(SpeechRecognitionRecognizerImplTest, OnLanguagePackInstalledTest) {
  CreateRecognizer(CreateOptions());
  EXPECT_CALL(*soda_client_, Reset(_, _, _));
  recognizer_->OnLanguagePackInstalled(config_paths());

  auto* config = recognizer_->GetExtendedSodaConfigMsgForTesting();
  EXPECT_EQ(soda::chrome::ExtendedSodaConfigMsg::CAPTION,
            config->recognition_mode());
  EXPECT_FALSE(config->enable_formatting());
  EXPECT_TRUE(config->mask_offensive_words());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       SpeechRecognitionRecognitionContextTest) {
  std::vector<media::SpeechRecognitionPhrase> phrases;
  phrases.emplace_back("test phrase", 2.0);

  CreateRecognizer(
      CreateOptions(media::SpeechRecognitionRecognitionContext(phrases)));
  recognizer_->OnLanguagePackInstalled(config_paths());

  auto context =
      recognizer_->GetExtendedSodaConfigMsgForTesting()->recognition_context();
  EXPECT_EQ(1, context.context().size());
  auto context_input = context.context().Get(0);
  EXPECT_EQ("android-speech-api-generic-phrases", context_input.name());
  EXPECT_EQ(1, context_input.phrases().phrase().size());
  auto phrase = context_input.phrases().phrase().Get(0);
  EXPECT_EQ("test phrase", phrase.phrase());
  EXPECT_EQ(2.0, phrase.boost());
}

TEST_F(SpeechRecognitionRecognizerImplTest, UpdateRecognitionContextTest) {
  CreateRecognizer(CreateOptions());
  media::SpeechRecognitionRecognitionContext context;
  context.phrases.emplace_back("test phrase", 2.0);
  EXPECT_CALL(*soda_client_, UpdateRecognitionContext(_));
  recognizer_->UpdateRecognitionContext(context);
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       PopulatesTimestampsForFinalResults) {
  CreateRecognizer(CreateOptions());

  // 1. Populate the timestamp estimator.
  // Audio from media time [10s, 12s) corresponds to speech time [0s, 2s).
  SendAudio(recognizer_.get(), base::Seconds(2), base::Seconds(10));

  // 2. Create a final recognition result for speech time [0s, 1.5s).
  media::SpeechRecognitionResult result;
  result.transcription = "hello world";
  result.is_final = true;
  result.timing_information = media::TimingInformation();
  result.timing_information->audio_start_time = base::Seconds(0);
  result.timing_information->audio_end_time = base::Milliseconds(1500);

  // 3. Trigger the event handler to receive the result.
  recognizer_->recognition_event_callback().Run(std::move(result));
  WaitForRecognitionEvent();

  // 4. Verify the timestamps on the received result.
  ASSERT_TRUE(last_received_result_.timing_information.has_value());
  const auto& timestamps =
      last_received_result_.timing_information->originating_media_timestamps;
  ASSERT_TRUE(timestamps.has_value());
  ASSERT_EQ(timestamps->size(), 1u);
  // Should correspond to media time [10s, 11.5s).
  EXPECT_EQ((*timestamps)[0].start, base::Seconds(10));
  EXPECT_EQ((*timestamps)[0].end, base::Seconds(10) + base::Milliseconds(1500));
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       PopulatesTimestampsForNonFinalResults) {
  CreateRecognizer(CreateOptions());

  // 1. Populate the timestamp estimator.
  // Audio from media time [20s, 25s) corresponds to speech time [0s, 5s).
  SendAudio(recognizer_.get(), base::Seconds(5), base::Seconds(20));

  // 2. Create a non-final recognition result for speech time [1s, 3s).
  media::SpeechRecognitionResult result;
  result.transcription = "testing";
  result.is_final = false;
  result.timing_information = media::TimingInformation();
  result.timing_information->audio_start_time = base::Seconds(1);
  result.timing_information->audio_end_time = base::Seconds(3);

  // 3. Trigger the event handler to receive the result.
  recognizer_->recognition_event_callback().Run(std::move(result));
  WaitForRecognitionEvent();

  // 4. Verify the timestamps on the received result.
  ASSERT_TRUE(last_received_result_.timing_information.has_value());
  const auto& timestamps =
      last_received_result_.timing_information->originating_media_timestamps;
  ASSERT_TRUE(timestamps.has_value());
  ASSERT_EQ(timestamps->size(), 1u);
  // Should correspond to media time [21s, 23s).
  EXPECT_EQ((*timestamps)[0].start, base::Seconds(21));
  EXPECT_EQ((*timestamps)[0].end, base::Seconds(23));

  // 5. Verify the estimator's state was NOT changed.
  // We can do this by sending a final result for the full range [0s, 5s)
  // and checking that it returns the full media range [20s, 25s).
  // If Peek had mutated the state, this would fail.
  media::SpeechRecognitionResult final_result;
  final_result.is_final = true;
  final_result.timing_information = media::TimingInformation();
  final_result.timing_information->audio_start_time = base::Seconds(0);
  final_result.timing_information->audio_end_time = base::Seconds(5);

  recognizer_->recognition_event_callback().Run(std::move(final_result));
  WaitForRecognitionEvent();

  ASSERT_TRUE(last_received_result_.timing_information.has_value());
  const auto& final_timestamps =
      last_received_result_.timing_information->originating_media_timestamps;
  ASSERT_TRUE(final_timestamps.has_value());
  ASSERT_EQ(final_timestamps->size(), 1u);
  EXPECT_EQ((*final_timestamps)[0].start, base::Seconds(20));
  EXPECT_EQ((*final_timestamps)[0].end, base::Seconds(25));
}

}  // namespace speech
