// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_recognizer_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/services/speech/soda/mock_soda_client.h"
#include "components/soda/buildflags.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_SODA_INTEGRATION_TESTS)
#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "chrome/services/speech/soda/soda_test_paths.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#endif

using ::testing::_;
using ::testing::NiceMock;

namespace speech {

namespace {

constexpr char kPrimaryLanguageName[] = "en-US";
constexpr char kPrimaryLanguageNameLowerCase[] = "en-us";
constexpr char kPrimaryLanguageNameGeneric[] = "en";
constexpr char kPrimaryLanguageNameGenericUpperCase[] = "EN";
constexpr char kSodaTfLiteDelegateStatusHistogram[] =
    "Accessibility.LiveCaption.SodaTfLiteDelegateStatus";

using GetCreateCountFn = int (*)();

DISABLE_CFI_DLSYM int CallGetCreateCount(GetCreateCountFn get_create_count) {
  return get_create_count();
}

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
    ++recognition_event_count_;
    std::move(reply).Run(true);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
  void OnSpeechRecognitionStopped() override { ++stopped_count_; }
  void OnSpeechRecognitionError() override {}
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {
    ++language_identification_event_count_;
  }

  void CreateRecognizer(media::mojom::SpeechRecognitionOptionsPtr options,
                        const std::string& primary_language_name) {
    recognizer_ = std::make_unique<SpeechRecognitionRecognizerImpl>(
        receiver_.BindNewPipeAndPassRemote(), std::move(options),
        base::FilePath(), config_paths_, primary_language_name,
        /*mask_offensive_words=*/true);
    auto soda_client = std::make_unique<NiceMock<::soda::MockSodaClient>>();
    soda_client_ = soda_client.get();
    ON_CALL(*soda_client_, IsInitialized())
        .WillByDefault(testing::Return(true));
    recognizer_->SetSodaClientForTesting(std::move(soda_client));
  }

  void CreateRecognizerWithSodaLibrary(
      media::mojom::SpeechRecognitionOptionsPtr options,
      const base::FilePath& library_path) {
    recognizer_ = std::make_unique<SpeechRecognitionRecognizerImpl>(
        receiver_.BindNewPipeAndPassRemote(), std::move(options),
        base::FilePath(), config_paths_, kPrimaryLanguageName,
        /*mask_offensive_words=*/true);
    recognizer_->CreateSodaClient(library_path);
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

  media::mojom::SpeechRecognitionOptionsPtr CreateOptionsSkippingEmptyAudio() {
    auto options = CreateOptions();
    options->skip_continuously_empty_audio = true;
    return options;
  }

  base::flat_map<std::string, base::FilePath> config_paths() {
    return config_paths_;
  }

 protected:
  static constexpr int kSampleRate = 16000;

  // Sends a TFLITE_DELEGATE_INFO response through the SODA callback, with the
  // XNNPACK delegate feature in the given state.
  void SendTfLiteDelegateInfo(bool delegate_requested,
                              int tflite_session_count,
                              int delegated_session_count) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(media::kSodaXnnpackDelegate,
                                      delegate_requested);
    CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

    SerializedSodaConfig saved_config;
    EXPECT_CALL(*soda_client_, Reset)
        .WillOnce(testing::SaveArg<0>(&saved_config));
    recognizer_->OnLanguagePackInstalled(config_paths());
    ASSERT_EQ(delegate_requested,
              recognizer_->GetExtendedSodaConfigMsgForTesting()
                  ->enable_xnnpack_delegate());

    soda::chrome::SodaResponse response;
    response.set_soda_type(soda::chrome::SodaResponse::TFLITE_DELEGATE_INFO);
    auto* info = response.mutable_tflite_delegate_info();
    info->set_tflite_session_count(tflite_session_count);
    info->set_delegated_session_count(delegated_session_count);
    std::string serialized;
    response.SerializeToString(&serialized);

    saved_config.callback(serialized.c_str(), serialized.size(),
                          saved_config.callback_handle);
  }

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

  void SendAudioSamples(const std::vector<int16_t>& samples, int sample_rate) {
    auto audio_buffer = media::mojom::AudioDataS16::New();
    audio_buffer->sample_rate = sample_rate;
    audio_buffer->channel_count = 1;
    audio_buffer->frame_count = samples.size();
    audio_buffer->data = samples;
    recognizer_->SendAudioToSpeechRecognitionService(std::move(audio_buffer),
                                                     std::nullopt);
  }

  void SendNonEmptyAudio() {
    SendAudioSamples(std::vector<int16_t>(kSampleRate, 1), kSampleRate);
  }

  void SendEmptyAudio() {
    SendAudioSamples(std::vector<int16_t>(kSampleRate, 0), kSampleRate);
  }

  void SendEmptyAudioPastSilenceThreshold() {
    task_environment_.FastForwardBy(base::Seconds(11));
    SendEmptyAudio();
  }

  // Configures `soda_client_` to emit STOP on `soda_thread_` on Destroy() and
  // MarkDone(), matching the real SODA library.
  void EmulateSodaLibrary() {
    ASSERT_TRUE(soda_thread_.Start());
    ON_CALL(*soda_client_, IsInitialized()).WillByDefault([this]() {
      return soda_initialized_;
    });
    ON_CALL(*soda_client_, Reset(_, _, _))
        .WillByDefault([this](const SerializedSodaConfig config, int, int) {
          StopEmulatedSodaAndWait();
          soda_config_ = config;
          soda_config_.soda_config = nullptr;
          soda_config_.soda_config_size = 0;
          soda_initialized_ = true;
          soda_running_ = true;
        });
    ON_CALL(*soda_client_, Destroy()).WillByDefault([this]() {
      StopEmulatedSodaAndWait();
      soda_initialized_ = false;
    });
    ON_CALL(*soda_client_, MarkDone()).WillByDefault([this]() {
      if (std::exchange(soda_running_, false)) {
        PostSodaResponse(soda_config_, StopResponse());
      }
    });
  }

  void StopEmulatedSodaAndWait() {
    if (std::exchange(soda_running_, false)) {
      PostSodaResponse(soda_config_, StopResponse());
      soda_thread_.FlushForTesting();
    }
  }

  static soda::chrome::SodaResponse StopResponse() {
    soda::chrome::SodaResponse response;
    response.set_soda_type(soda::chrome::SodaResponse::STOP);
    return response;
  }

  static soda::chrome::SodaResponse RecognitionResponse(
      const std::string& transcription) {
    soda::chrome::SodaResponse response;
    response.set_soda_type(soda::chrome::SodaResponse::RECOGNITION);
    auto* result = response.mutable_recognition_result();
    result->set_result_type(soda::chrome::SodaRecognitionResult::FINAL);
    result->add_hypothesis(transcription);
    return response;
  }

  void PostSodaResponse(const SerializedSodaConfig& config,
                        const soda::chrome::SodaResponse& response) {
    soda_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](SerializedSodaConfig config, const std::string& serialized) {
              config.callback(serialized.c_str(), serialized.size(),
                              config.callback_handle);
            },
            config, response.SerializeAsString()));
  }

  // Flushes `soda_thread_` and waits for queued client events to be delivered.
  void FlushSodaAndClientEvents() {
    if (soda_thread_.IsRunning()) {
      soda_thread_.FlushForTesting();
    }
    const int expected_count = language_identification_event_count_ + 1;
    recognizer_->language_identification_event_callback().Run(
        kPrimaryLanguageName, media::mojom::ConfidenceLevel::kUnknown,
        media::mojom::AsrSwitchResult::kDefaultNoSwitch);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return language_identification_event_count_ == expected_count;
    }));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::Thread soda_thread_{"FakeSodaThread"};
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient> receiver_{
      this};
  base::flat_map<std::string, base::FilePath> config_paths_;
  media::SpeechRecognitionResult last_received_result_;
  int recognition_event_count_ = 0;
  int language_identification_event_count_ = 0;
  int stopped_count_ = 0;
  SerializedSodaConfig soda_config_ = {};
  bool soda_initialized_ = false;
  bool soda_running_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<SpeechRecognitionRecognizerImpl> recognizer_;
  raw_ptr<NiceMock<::soda::MockSodaClient>> soda_client_ = nullptr;
};

TEST_F(SpeechRecognitionRecognizerImplTest, OnLanguagePackInstalledTest) {
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);
  EXPECT_CALL(*soda_client_, Reset(_, _, _));
  recognizer_->OnLanguagePackInstalled(config_paths());

  auto* config = recognizer_->GetExtendedSodaConfigMsgForTesting();
  EXPECT_EQ(soda::chrome::ExtendedSodaConfigMsg::CAPTION,
            config->recognition_mode());
  EXPECT_FALSE(config->enable_formatting());
  EXPECT_TRUE(config->mask_offensive_words());
}

TEST_F(SpeechRecognitionRecognizerImplTest, XnnpackDelegateFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kSodaXnnpackDelegate);
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);
  EXPECT_CALL(*soda_client_, Reset(_, _, _));
  recognizer_->OnLanguagePackInstalled(config_paths());

  EXPECT_TRUE(recognizer_->GetExtendedSodaConfigMsgForTesting()
                  ->enable_xnnpack_delegate());
}

TEST_F(SpeechRecognitionRecognizerImplTest, XnnpackDelegateFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(media::kSodaXnnpackDelegate);
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);
  EXPECT_CALL(*soda_client_, Reset(_, _, _));
  recognizer_->OnLanguagePackInstalled(config_paths());

  // The field must stay unset so the serialized config is unchanged.
  EXPECT_FALSE(recognizer_->GetExtendedSodaConfigMsgForTesting()
                   ->has_enable_xnnpack_delegate());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       OnLanguagePackInstalledTest_MultiLanguageEnabled) {
  base::flat_map<std::string, base::FilePath> multiple_config_paths;
  multiple_config_paths[kPrimaryLanguageName] =
      base::FilePath::FromASCII("/fake/path");
  multiple_config_paths["fr-FR"] = base::FilePath::FromASCII("/fake/path2");

  auto options = CreateOptions();
  options->allow_multi_language = true;
  CreateRecognizer(std::move(options), kPrimaryLanguageName);
  EXPECT_CALL(*soda_client_, Reset(_, _, _));
  recognizer_->OnLanguagePackInstalled(multiple_config_paths);

  auto* config = recognizer_->GetExtendedSodaConfigMsgForTesting();
  EXPECT_EQ(soda::chrome::ExtendedSodaConfigMsg::CAPTION,
            config->recognition_mode());
  EXPECT_TRUE(config->has_multilang_config());
  EXPECT_EQ(
      2u,
      config->multilang_config().multilang_language_pack_directory().size());
  EXPECT_EQ("/fake/path", config->language_pack_directory());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       OnLanguagePackInstalledTest_MultiLanguageDisabled) {
  base::flat_map<std::string, base::FilePath> multiple_config_paths;
  multiple_config_paths[kPrimaryLanguageName] =
      base::FilePath::FromASCII("/fake/path");
  multiple_config_paths["fr-FR"] = base::FilePath::FromASCII("/fake/path2");

  auto options = CreateOptions();
  options->allow_multi_language = false;
  CreateRecognizer(std::move(options), kPrimaryLanguageName);
  EXPECT_CALL(*soda_client_, Reset(_, _, _));
  recognizer_->OnLanguagePackInstalled(multiple_config_paths);

  auto* config = recognizer_->GetExtendedSodaConfigMsgForTesting();
  EXPECT_EQ(soda::chrome::ExtendedSodaConfigMsg::CAPTION,
            config->recognition_mode());
  EXPECT_FALSE(config->has_multilang_config());
  EXPECT_EQ("/fake/path", config->language_pack_directory());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       SpeechRecognitionRecognitionContextTest) {
  std::vector<media::SpeechRecognitionPhrase> phrases;
  phrases.emplace_back("test phrase", 2.0);
  CreateRecognizer(
      CreateOptions(media::SpeechRecognitionRecognitionContext(phrases)),
      kPrimaryLanguageName);
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
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);
  media::SpeechRecognitionRecognitionContext context;
  context.phrases.emplace_back("test phrase", 2.0);
  EXPECT_CALL(*soda_client_, UpdateRecognitionContext(_));
  recognizer_->UpdateRecognitionContext(context);
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       PopulatesTimestampsForFinalResults) {
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

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
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

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

TEST_F(SpeechRecognitionRecognizerImplTest,
       HandlesResultsWithInvalidAudioEndTime) {
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

  // 1. Populate the timestamp estimator.
  // Audio from media time [10s, 12s) corresponds to speech time [0s, 2s).
  SendAudio(recognizer_.get(), base::Seconds(2), base::Seconds(10));

  // 2. Create a non-final recognition result for speech time [1s, 1s).
  media::SpeechRecognitionResult result;
  result.transcription = "hello world";
  result.is_final = false;
  result.timing_information = media::TimingInformation();
  result.timing_information->audio_start_time = base::Seconds(1);
  result.timing_information->audio_end_time = base::Seconds(1);

  // 3. Trigger the event handler to receive the result.
  recognizer_->recognition_event_callback().Run(std::move(result));
  WaitForRecognitionEvent();

  // 4. Verify the timestamps on the received result.
  ASSERT_TRUE(last_received_result_.timing_information.has_value());
  auto& timestamps =
      last_received_result_.timing_information->originating_media_timestamps;
  ASSERT_FALSE(timestamps.has_value());

  // 5. Create a final recognition result.
  media::SpeechRecognitionResult final_result;
  final_result.is_final = true;
  final_result.timing_information = media::TimingInformation();
  final_result.timing_information->audio_start_time = base::Seconds(2);
  final_result.timing_information->audio_end_time = base::Seconds(2);

  // 6. Trigger the event handler to receive the result.
  recognizer_->recognition_event_callback().Run(std::move(final_result));
  WaitForRecognitionEvent();

  // 7. Verify the timestamps on the received result.
  ASSERT_TRUE(last_received_result_.timing_information.has_value());
  timestamps =
      last_received_result_.timing_information->originating_media_timestamps;
  ASSERT_FALSE(timestamps.has_value());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       HandlesResultsWithInvertedAndNegativeAudioTimestamps) {
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

  // Audio from media time [10s, 12s) corresponds to speech time [0s, 2s).
  SendAudio(recognizer_.get(), base::Seconds(2), base::Seconds(10));

  // 1. Inverted timestamps (`audio_end_time < audio_start_time`).
  // Without clamping, Mojo deserialization fails with
  // VALIDATION_ERROR_DESERIALIZATION_FAILED.
  media::SpeechRecognitionResult inverted_result;
  inverted_result.transcription = "inverted";
  inverted_result.is_final = false;
  inverted_result.timing_information = media::TimingInformation();
  inverted_result.timing_information->audio_start_time = base::Seconds(2);
  inverted_result.timing_information->audio_end_time = base::Seconds(1);

  recognizer_->recognition_event_callback().Run(std::move(inverted_result));
  WaitForRecognitionEvent();

  ASSERT_TRUE(last_received_result_.timing_information.has_value());
  EXPECT_EQ(last_received_result_.timing_information->audio_start_time,
            base::Seconds(2));
  EXPECT_EQ(last_received_result_.timing_information->audio_end_time,
            base::Seconds(2));
  EXPECT_FALSE(last_received_result_.timing_information
                   ->originating_media_timestamps.has_value());

  // 2. Negative start timestamp (`audio_start_time < 0`, `audio_end_time > 0`).
  // Should clamp `audio_start_time` to 0s and compute media timestamps for
  // [0s, 2s) -> [10s, 12s).
  media::SpeechRecognitionResult negative_start_result;
  negative_start_result.transcription = "negative start";
  negative_start_result.is_final = true;
  negative_start_result.timing_information = media::TimingInformation();
  negative_start_result.timing_information->audio_start_time =
      base::Seconds(-1);
  negative_start_result.timing_information->audio_end_time = base::Seconds(2);
  negative_start_result.timing_information->hypothesis_parts =
      std::vector<media::HypothesisParts>{
          media::HypothesisParts({"negative"}, base::Seconds(-1)),
          media::HypothesisParts({"start"}, base::Seconds(5))};

  recognizer_->recognition_event_callback().Run(
      std::move(negative_start_result));
  WaitForRecognitionEvent();

  ASSERT_TRUE(last_received_result_.timing_information.has_value());
  EXPECT_EQ(last_received_result_.timing_information->audio_start_time,
            base::Seconds(0));
  EXPECT_EQ(last_received_result_.timing_information->audio_end_time,
            base::Seconds(2));
  ASSERT_TRUE(last_received_result_.timing_information
                  ->originating_media_timestamps.has_value());
  ASSERT_EQ(last_received_result_.timing_information
                ->originating_media_timestamps->size(),
            1u);
  EXPECT_EQ((*last_received_result_.timing_information
                  ->originating_media_timestamps)[0]
                .start,
            base::Seconds(10));
  EXPECT_EQ((*last_received_result_.timing_information
                  ->originating_media_timestamps)[0]
                .end,
            base::Seconds(12));
  ASSERT_TRUE(
      last_received_result_.timing_information->hypothesis_parts.has_value());
  ASSERT_EQ(last_received_result_.timing_information->hypothesis_parts->size(),
            2u);
  EXPECT_EQ((*last_received_result_.timing_information->hypothesis_parts)[0]
                .hypothesis_part_offset,
            base::Seconds(0));
  EXPECT_EQ((*last_received_result_.timing_information->hypothesis_parts)[1]
                .hypothesis_part_offset,
            base::Seconds(2));
}

TEST_F(SpeechRecognitionRecognizerImplTest, ResetSodaWithDifferentCaseLang) {
  config_paths_[kPrimaryLanguageName] = base::FilePath::FromASCII("/fake/path");
  CreateRecognizer(CreateOptions(), kPrimaryLanguageNameLowerCase);
  EXPECT_CALL(*soda_client_, Reset(_, _, _));
  recognizer_->OnLanguagePackInstalled(config_paths());

  auto* config = recognizer_->GetExtendedSodaConfigMsgForTesting();
  EXPECT_EQ(soda::chrome::ExtendedSodaConfigMsg::CAPTION,
            config->recognition_mode());
  EXPECT_EQ("/fake/path", config->language_pack_directory());
}

TEST_F(SpeechRecognitionRecognizerImplTest, ResetSodaWithGenericLang) {
  config_paths_[kPrimaryLanguageName] = base::FilePath::FromASCII("/fake/path");
  CreateRecognizer(CreateOptions(), kPrimaryLanguageNameGeneric);
  EXPECT_CALL(*soda_client_, Reset(_, _, _));
  recognizer_->OnLanguagePackInstalled(config_paths());

  auto* config = recognizer_->GetExtendedSodaConfigMsgForTesting();
  EXPECT_EQ(soda::chrome::ExtendedSodaConfigMsg::CAPTION,
            config->recognition_mode());
  EXPECT_EQ("/fake/path", config->language_pack_directory());
}

TEST_F(SpeechRecognitionRecognizerImplTest, ResetSodaWithGenericUpperCaseLang) {
  config_paths_[kPrimaryLanguageName] = base::FilePath::FromASCII("/fake/path");
  CreateRecognizer(CreateOptions(), kPrimaryLanguageNameGenericUpperCase);
  EXPECT_CALL(*soda_client_, Reset(_, _, _));
  recognizer_->OnLanguagePackInstalled(config_paths());

  auto* config = recognizer_->GetExtendedSodaConfigMsgForTesting();
  EXPECT_EQ(soda::chrome::ExtendedSodaConfigMsg::CAPTION,
            config->recognition_mode());
  EXPECT_EQ("/fake/path", config->language_pack_directory());
}

TEST_F(SpeechRecognitionRecognizerImplTest, SodaClientRegistryThreadSafety) {
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

  SerializedSodaConfig saved_config;
  EXPECT_CALL(*soda_client_, Reset(_, _, _))
      .WillOnce(testing::SaveArg<0>(&saved_config));

  recognizer_->OnLanguagePackInstalled(config_paths());

  // Create a valid dummy response so the callback proceeds to fetching from the
  // registry.
  soda::chrome::SodaResponse response;
  response.set_soda_type(soda::chrome::SodaResponse::RECOGNITION);
  auto* result = response.mutable_recognition_result();
  result->set_result_type(soda::chrome::SodaRecognitionResult::FINAL);
  result->add_hypothesis("UAF test");
  std::string serialized;
  response.SerializeToString(&serialized);

  constexpr int kNumThreads = 20;
  base::WaitableEvent threads_started_event;
  base::RepeatingClosure barrier = base::BarrierClosure(
      kNumThreads, base::BindOnce(&base::WaitableEvent::Signal,
                                  base::Unretained(&threads_started_event)));

  // Launch background tasks to pound the callback.
  for (int i = 0; i < kNumThreads; i++) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](SerializedSodaConfig config, std::string serialized,
               base::RepeatingClosure barrier) {
              barrier.Run();
              for (int j = 0; j < 100; j++) {
                config.callback(serialized.c_str(), serialized.size(),
                                config.callback_handle);
              }
            },
            saved_config, serialized, barrier));
  }

  threads_started_event.Wait();

  // Clear the raw_ptr before destroying the recognizer to avoid dangling
  // pointer warnings.
  soda_client_ = nullptr;

  // Concurrently destroy the recognizer.
  recognizer_.reset();

  task_environment_.RunUntilIdle();
}

TEST_F(SpeechRecognitionRecognizerImplTest, EmptyHypothesisDoesNotCrash) {
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

  SerializedSodaConfig saved_config;
  EXPECT_CALL(*soda_client_, Reset(_, _, _))
      .WillOnce(testing::SaveArg<0>(&saved_config));

  recognizer_->OnLanguagePackInstalled(config_paths());

  soda::chrome::SodaResponse response;
  response.set_soda_type(soda::chrome::SodaResponse::RECOGNITION);
  auto* result = response.mutable_recognition_result();
  result->set_result_type(soda::chrome::SodaRecognitionResult::FINAL);
  // Do not add any hypothesis to verify bounds check prevents crash.
  std::string serialized;
  response.SerializeToString(&serialized);

  saved_config.callback(serialized.c_str(), serialized.size(),
                        saved_config.callback_handle);

  EXPECT_EQ(0, recognition_event_count_);
  EXPECT_TRUE(last_received_result_.transcription.empty());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       RecordsNotDelegatedWhenNotRequested) {
  base::HistogramTester histogram_tester;
  SendTfLiteDelegateInfo(/*delegate_requested=*/false,
                         /*tflite_session_count=*/4,
                         /*delegated_session_count=*/0);
  histogram_tester.ExpectUniqueSample(kSodaTfLiteDelegateStatusHistogram,
                                      /*kNotDelegated*/ 0, 1);
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       RecordsSuccessWhenAllSessionsDelegated) {
  base::HistogramTester histogram_tester;
  SendTfLiteDelegateInfo(/*delegate_requested=*/true,
                         /*tflite_session_count=*/4,
                         /*delegated_session_count=*/4);
  histogram_tester.ExpectUniqueSample(kSodaTfLiteDelegateStatusHistogram,
                                      /*kDelegateSuccess*/ 1, 1);
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       RecordsFallbackWhenSomeSessionsNotDelegated) {
  base::HistogramTester histogram_tester;
  SendTfLiteDelegateInfo(/*delegate_requested=*/true,
                         /*tflite_session_count=*/4,
                         /*delegated_session_count=*/3);
  histogram_tester.ExpectUniqueSample(kSodaTfLiteDelegateStatusHistogram,
                                      /*kDelegateFallback*/ 2, 1);
}

TEST_F(SpeechRecognitionRecognizerImplTest, SkipsRecordingWithoutSessions) {
  base::HistogramTester histogram_tester;
  SendTfLiteDelegateInfo(/*delegate_requested=*/true,
                         /*tflite_session_count=*/0,
                         /*delegated_session_count=*/0);
  histogram_tester.ExpectTotalCount(kSodaTfLiteDelegateStatusHistogram, 0);
}

TEST_F(SpeechRecognitionRecognizerImplTest, DestroySodaWhenClientDisconnects) {
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

  // Disconnecting the client remote should release the SODA instance and
  // prevent subsequent ResetSoda() calls from re-initializing it.
  base::RunLoop run_loop;
  EXPECT_CALL(*soda_client_, Destroy()).WillOnce([&]() { run_loop.Quit(); });
  receiver_.reset();
  run_loop.Run();

  EXPECT_CALL(*soda_client_, Reset(_, _, _)).Times(0);
  recognizer_->OnLanguagePackInstalled(config_paths());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       DoNotResetUninitializedSodaOnLanguagePackInstalled) {
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);
  ON_CALL(*soda_client_, IsInitialized()).WillByDefault(testing::Return(false));

  base::flat_map<std::string, base::FilePath> updated_paths;
  updated_paths[kPrimaryLanguageName] = base::FilePath::FromASCII("/new/path");

  // Updating language pack paths while SODA is uninitialized should not
  // eagerly initialize SODA.
  EXPECT_CALL(*soda_client_, Reset(_, _, _)).Times(0);
  recognizer_->OnLanguagePackInstalled(updated_paths);

  // When audio finally arrives, SODA should be initialized with the updated
  // config paths.
  EXPECT_CALL(*soda_client_, Reset(_, _, _)).Times(1);
  SendAudio(recognizer_.get(), base::Seconds(1), base::Seconds(0));
  EXPECT_EQ("/new/path", recognizer_->GetExtendedSodaConfigMsgForTesting()
                             ->language_pack_directory());
}

TEST_F(SpeechRecognitionRecognizerImplTest, DestroySodaOnContinuousSilence) {
  auto options = CreateOptions();
  options->skip_continuously_empty_audio = true;
  CreateRecognizer(std::move(options), kPrimaryLanguageName);

  // Send silent audio within the 10-second threshold; SODA should not be
  // destroyed yet.
  EXPECT_CALL(*soda_client_, Destroy()).Times(0);
  SendAudio(recognizer_.get(), base::Seconds(1), base::Seconds(0));

  // Advance past the 10-second silence threshold and send another silent
  // buffer; SODA should be destroyed and no audio forwarded to SODA.
  task_environment_.FastForwardBy(base::Seconds(11));
  EXPECT_CALL(*soda_client_, Destroy()).Times(1);
  EXPECT_CALL(*soda_client_, AddAudio(_, _)).Times(0);
  SendAudio(recognizer_.get(), base::Seconds(1), base::Seconds(11));
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       OnLanguageChangedToInstalledLanguage) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  config_paths_[kPrimaryLanguageName] = base::FilePath::FromASCII("/fake/en");
  config_paths_["fr-FR"] = temp_dir.GetPath();
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

  base::RunLoop run_loop;
  EXPECT_CALL(*soda_client_, Reset(_, _, _)).WillOnce([&]() {
    run_loop.Quit();
  });
  recognizer_->OnLanguageChanged("fr-FR");
  run_loop.Run();

  EXPECT_EQ(temp_dir.GetPath().AsUTF8Unsafe(),
            recognizer_->GetExtendedSodaConfigMsgForTesting()
                ->language_pack_directory());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       OnLanguageChangedToUninstalledLanguage) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  config_paths_[kPrimaryLanguageName] = base::FilePath::FromASCII("/fake/en");
  config_paths_["fr-FR"] = temp_dir.GetPath().AppendASCII("nonexistent");
  config_paths_["de-DE"] = temp_dir.GetPath();
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

  // Switching to a language whose pack is not in `config_paths_` ("es-ES") or
  // whose directory does not exist on disk ("fr-FR") should not reset SODA or
  // change the active language. Follow with a valid switch ("de-DE") on the
  // same sequenced `task_runner_` to flush the "fr-FR" check deterministically.
  base::RunLoop run_loop;
  EXPECT_CALL(*soda_client_, Reset(_, _, _)).WillOnce([&]() {
    run_loop.Quit();
  });
  recognizer_->OnLanguageChanged("es-ES");
  recognizer_->OnLanguageChanged("fr-FR");
  recognizer_->OnLanguageChanged("de-DE");
  run_loop.Run();

  EXPECT_EQ("de-DE", recognizer_->primary_language_name());
  EXPECT_EQ(temp_dir.GetPath().AsUTF8Unsafe(),
            recognizer_->GetExtendedSodaConfigMsgForTesting()
                ->language_pack_directory());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       OnLanguageChangedToSameOrInvalidLanguage) {
  config_paths_[kPrimaryLanguageName] = base::FilePath::FromASCII("/fake/en");
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);

  EXPECT_CALL(*soda_client_, Reset(_, _, _)).Times(0);
  recognizer_->OnLanguageChanged(kPrimaryLanguageName);
  recognizer_->OnLanguageChanged("invalid-language");
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       DoNotResetUninitializedSodaOnLanguageChanged) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  config_paths_[kPrimaryLanguageName] = base::FilePath::FromASCII("/fake/en");
  config_paths_["fr-FR"] = temp_dir.GetPath();
  CreateRecognizer(CreateOptions(), kPrimaryLanguageName);
  ON_CALL(*soda_client_, IsInitialized()).WillByDefault(testing::Return(false));

  // Changing language while SODA is uninitialized should update the primary
  // language without eagerly initializing SODA.
  EXPECT_CALL(*soda_client_, Reset(_, _, _)).Times(0);
  recognizer_->OnLanguageChanged("fr-FR");
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return recognizer_->primary_language_name() == "fr-FR"; }));

  // When audio arrives, SODA should be initialized with the new language.
  EXPECT_CALL(*soda_client_, Reset(_, _, _)).Times(1);
  SendAudio(recognizer_.get(), base::Seconds(1), base::Seconds(0));
  EXPECT_EQ(temp_dir.GetPath().AsUTF8Unsafe(),
            recognizer_->GetExtendedSodaConfigMsgForTesting()
                ->language_pack_directory());
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       SilenceReleaseDoesNotNotifyClientOfStop) {
  CreateRecognizer(CreateOptionsSkippingEmptyAudio(), kPrimaryLanguageName);
  EmulateSodaLibrary();

  SendNonEmptyAudio();
  ASSERT_TRUE(soda_initialized_);

  EXPECT_CALL(*soda_client_, Destroy()).Times(1);
  EXPECT_CALL(*soda_client_, AddAudio(_, _)).Times(0);
  SendEmptyAudioPastSilenceThreshold();
  FlushSodaAndClientEvents();
  EXPECT_FALSE(soda_initialized_);
  EXPECT_EQ(0, stopped_count_);
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       ResultsQueuedBeforeReleaseAreStillDelivered) {
  CreateRecognizer(CreateOptionsSkippingEmptyAudio(), kPrimaryLanguageName);
  EmulateSodaLibrary();

  SendNonEmptyAudio();
  task_environment_.FastForwardBy(base::Seconds(11));

  // Queue a result before SODA is released for silence.
  PostSodaResponse(soda_config_, RecognitionResponse("queued"));
  soda_thread_.FlushForTesting();
  ASSERT_EQ(0, recognition_event_count_);

  SendEmptyAudio();
  EXPECT_FALSE(soda_initialized_);

  FlushSodaAndClientEvents();
  EXPECT_EQ(1, recognition_event_count_);
  EXPECT_EQ("queued", last_received_result_.transcription);
  EXPECT_EQ(0, stopped_count_);
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       ResumesRecognitionAcrossSilenceCycles) {
  CreateRecognizer(CreateOptionsSkippingEmptyAudio(), kPrimaryLanguageName);
  EmulateSodaLibrary();

  EXPECT_CALL(*soda_client_, Reset(_, _, _)).Times(3);
  EXPECT_CALL(*soda_client_, Destroy()).Times(2);

  SendNonEmptyAudio();
  for (int cycle = 1; cycle <= 2; ++cycle) {
    SCOPED_TRACE(cycle);
    const SerializedSodaConfig released_soda_config = soda_config_;
    SendEmptyAudioPastSilenceThreshold();
    EXPECT_FALSE(soda_initialized_);

    SendNonEmptyAudio();
    EXPECT_TRUE(soda_initialized_);
    EXPECT_NE(released_soda_config.callback_handle,
              soda_config_.callback_handle);

    // Stale responses from the released instance are dropped.
    PostSodaResponse(released_soda_config, RecognitionResponse("stale"));
    PostSodaResponse(released_soda_config, StopResponse());
    FlushSodaAndClientEvents();
    EXPECT_EQ(cycle - 1, recognition_event_count_);

    PostSodaResponse(soda_config_, RecognitionResponse("fresh"));
    FlushSodaAndClientEvents();
    EXPECT_EQ(cycle, recognition_event_count_);
    EXPECT_EQ("fresh", last_received_result_.transcription);
  }
  EXPECT_EQ(0, stopped_count_);
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       MarkDoneWhileReleasedNotifiesClientOnce) {
  CreateRecognizer(CreateOptionsSkippingEmptyAudio(), kPrimaryLanguageName);
  EmulateSodaLibrary();

  SendNonEmptyAudio();
  SendEmptyAudioPastSilenceThreshold();
  FlushSodaAndClientEvents();
  ASSERT_FALSE(soda_initialized_);
  ASSERT_EQ(0, stopped_count_);

  EXPECT_CALL(*soda_client_, MarkDone()).Times(0);
  recognizer_->MarkDone();
  recognizer_->MarkDone();
  FlushSodaAndClientEvents();
  EXPECT_EQ(1, stopped_count_);

  // Audio arriving after MarkDone() is ignored.
  EXPECT_CALL(*soda_client_, Reset(_, _, _)).Times(0);
  EXPECT_CALL(*soda_client_, AddAudio(_, _)).Times(0);
  SendNonEmptyAudio();
  FlushSodaAndClientEvents();
  EXPECT_EQ(1, stopped_count_);
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       MarkDoneAfterInitialSilenceNotifiesClient) {
  CreateRecognizer(CreateOptionsSkippingEmptyAudio(), kPrimaryLanguageName);
  EmulateSodaLibrary();

  // Drop silent audio past the threshold before SODA is ever initialized.
  EXPECT_CALL(*soda_client_, Reset(_, _, _)).Times(0);
  SendEmptyAudioPastSilenceThreshold();
  ASSERT_FALSE(soda_initialized_);

  EXPECT_CALL(*soda_client_, MarkDone()).Times(0);
  recognizer_->MarkDone();
  FlushSodaAndClientEvents();
  EXPECT_EQ(1, stopped_count_);
}

TEST_F(SpeechRecognitionRecognizerImplTest,
       MarkDoneDuringSpeechForwardsSodaStop) {
  CreateRecognizer(CreateOptionsSkippingEmptyAudio(), kPrimaryLanguageName);
  EmulateSodaLibrary();

  SendNonEmptyAudio();
  EXPECT_CALL(*soda_client_, MarkDone()).Times(1);
  recognizer_->MarkDone();
  FlushSodaAndClientEvents();
  EXPECT_EQ(1, stopped_count_);

  // SODA is not released for silence after MarkDone().
  EXPECT_CALL(*soda_client_, Destroy()).Times(0);
  SendEmptyAudioPastSilenceThreshold();
  FlushSodaAndClientEvents();
  EXPECT_EQ(1, stopped_count_);
}

TEST_F(SpeechRecognitionRecognizerImplTest, SilenceReleaseWithRealSodaClient) {
  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));
  base::FilePath fake_lib_path = exe_dir.AppendASCII(
      base::GetNativeLibraryName("fake_soda_library_for_testing"));
  ASSERT_TRUE(base::PathExists(fake_lib_path));

  base::ScopedNativeLibrary fake_lib(fake_lib_path);
  ASSERT_TRUE(fake_lib.is_valid());
  auto get_create_count = reinterpret_cast<GetCreateCountFn>(
      fake_lib.GetFunctionPointer("GetFakeSodaLibraryCreateCount"));
  ASSERT_TRUE(get_create_count);
  const int initial_create_count = CallGetCreateCount(get_create_count);

  CreateRecognizerWithSodaLibrary(CreateOptionsSkippingEmptyAudio(),
                                  fake_lib_path);

  SendNonEmptyAudio();
  EXPECT_EQ(initial_create_count + 1, CallGetCreateCount(get_create_count));

  SendEmptyAudioPastSilenceThreshold();
  FlushSodaAndClientEvents();
  EXPECT_EQ(0, stopped_count_);

  SendNonEmptyAudio();
  EXPECT_EQ(initial_create_count + 2, CallGetCreateCount(get_create_count));

  recognizer_->MarkDone();
  FlushSodaAndClientEvents();
  EXPECT_EQ(1, stopped_count_);
}

#if BUILDFLAG(ENABLE_SODA_INTEGRATION_TESTS)

TEST_F(SpeechRecognitionRecognizerImplTest, SilenceReleaseWithRealSodaLibrary) {
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
  // TODO(crbug.com/40753481): Enable test once arm64 macOS binary is available
  // in CIPD.
  GTEST_SKIP()
      << "SODA test binary for arm64 macOS is currently being rolled in CIPD.";
#else
  base::FilePath test_data_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
  const base::FilePath soda_resources_dir =
      test_data_dir.Append(base::FilePath(::soda::kSodaResourcePath));
  const base::FilePath libsoda_path = soda_resources_dir.Append(
      base::FilePath(::soda::kSodaTestBinaryRelativePath));
  ASSERT_TRUE(base::PathExists(libsoda_path));
  config_paths_[kPrimaryLanguageName] = soda_resources_dir.Append(
      base::FilePath(::soda::kSodaLanguagePackRelativePath));
  ASSERT_TRUE(base::PathExists(config_paths_[kPrimaryLanguageName]));

  std::string wav_data;
  ASSERT_TRUE(base::ReadFileToString(soda_resources_dir.Append(base::FilePath(
                                         ::soda::kSodaTestAudioRelativePath)),
                                     &wav_data));
  auto handler = media::WavAudioHandler::Create(base::as_byte_span(wav_data));
  ASSERT_TRUE(handler);
  ASSERT_EQ(1, handler->GetNumChannels());
  auto bus = media::AudioBus::Create(handler->GetNumChannels(),
                                     handler->total_frames_for_testing());
  size_t frames_written = 0u;
  ASSERT_TRUE(handler->CopyTo(bus.get(), &frames_written));
  std::vector<int16_t> speech(bus->frames());
  bus->ToInterleaved<media::SignedInt16SampleTypeTraits>(speech);
  const int sample_rate = handler->GetSampleRate();

  CreateRecognizerWithSodaLibrary(CreateOptionsSkippingEmptyAudio(),
                                  libsoda_path);

  SendAudioSamples(speech, sample_rate);
  task_environment_.FastForwardBy(base::Seconds(11));
  SendAudioSamples(std::vector<int16_t>(sample_rate, 0), sample_rate);
  FlushSodaAndClientEvents();
  EXPECT_EQ(0, stopped_count_);

  SendAudioSamples(speech, sample_rate);
  SendAudioSamples(speech, sample_rate);
  recognizer_->MarkDone();
  ASSERT_TRUE(base::test::RunUntil([&]() { return stopped_count_ > 0; }));
  EXPECT_EQ(1, stopped_count_);
  EXPECT_TRUE(last_received_result_.is_final);
  std::string transcription =
      base::ToLowerASCII(last_received_result_.transcription);
  std::erase_if(transcription,
                [](char c) { return base::IsAsciiPunctuation(c); });
  EXPECT_EQ("hey google hey google", transcription);
#endif
}

#endif  // BUILDFLAG(ENABLE_SODA_INTEGRATION_TESTS)

}  // namespace speech
