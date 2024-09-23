// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/audio_source_fetcher_impl.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"
#include "chrome/services/speech/speech_recognition_service_impl.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

namespace {

// The server based recognition audio buffer duration is 100ms:
constexpr int kServerBasedRecognitionAudioSampleRate = 16000;
constexpr int kServerBasedRecognitionAudioFramesPerBuffer = 1600;

// The original audio buffer duration is 200ms:
constexpr int kOriginalSampleRate = 48000;
constexpr int kOriginalFramesPerBuffer = 9600;

constexpr char kServerBasedRecognitionSessionLength[] =
    "Ash.SpeechRecognitionSessionLength.ServerBased";
constexpr char kOnDeviceRecognitionSessionLength[] =
    "Ash.SpeechRecognitionSessionLength.OnDevice";

}  // namespace

class MockStreamFactory : public audio::FakeStreamFactory {
 public:
  MockStreamFactory() = default;
  ~MockStreamFactory() override = default;

  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool enable_agc,
      base::ReadOnlySharedMemoryRegion key_press_count_buffer,
      media::mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback created_callback) override {
    last_created_callback_ = std::move(created_callback);
  }

 private:
  // Keeps the `last_created_callback_` alive during test.
  CreateInputStreamCallback last_created_callback_;

  mojo::Receiver<media::mojom::AudioStreamFactory> receiver_{this};
};

using OnSendAudioToSpeechRecognitionCallback =
    base::OnceCallback<void(media::mojom::AudioDataS16Ptr buffer)>;

// A class to verify whether the  media::mojom::AudioDataS16 data captured by
// AudioSourceFetcherImpl.
class MockAudioSourceConsumer : public AudioSourceConsumer {
 public:
  MockAudioSourceConsumer() = default;
  ~MockAudioSourceConsumer() override = default;

  // AudioSourceConsumer:
  void AddAudio(media::mojom::AudioDataS16Ptr buffer) override {
    EXPECT_FALSE(is_audio_end_);
    std::move(on_send_audio_to_speech_recognition_callback_)
        .Run(std::move(buffer));
  }

  void OnAudioCaptureEnd() override { is_audio_end_ = true; }

  void OnAudioCaptureError() override {}

  void SetOnSendAudioToSpeechRecognitionCallback(
      OnSendAudioToSpeechRecognitionCallback callback) {
    on_send_audio_to_speech_recognition_callback_ = std::move(callback);
  }

 private:
  // Used to verify the media::mojom::AudioDataS16 content.
  OnSendAudioToSpeechRecognitionCallback
      on_send_audio_to_speech_recognition_callback_;
  bool is_audio_end_ = false;
};

class AudioSourceFetcherImplTest
    : public testing::TestWithParam<bool>,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  AudioSourceFetcherImplTest() { is_server_based_ = GetParam(); }
  ~AudioSourceFetcherImplTest() override = default;

  void SetUp() override {
    std::unique_ptr<MockAudioSourceConsumer> speech_recognition_recognizer =
        std::make_unique<MockAudioSourceConsumer>();
    speech_recognition_recognizer_ = speech_recognition_recognizer.get();
    audio_source_fetcher_ = std::make_unique<AudioSourceFetcherImpl>(
        std::move(speech_recognition_recognizer),
        /*is_multi_channel_supported=*/true,
        /*is_server_based=*/is_server_based_);
  }

 protected:
  bool is_server_based() const { return is_server_based_; }

  AudioSourceFetcherImpl* audio_source_fetcher() {
    return audio_source_fetcher_.get();
  }

  void SetOnSendAudioToSpeechRecognitionCallback(
      OnSendAudioToSpeechRecognitionCallback callback) {
    speech_recognition_recognizer_->SetOnSendAudioToSpeechRecognitionCallback(
        std::move(callback));
  }

  void VerifyAudioBuffer(int sample_rate, int frame_count, bool stop = false) {
    base::RunLoop run_loop;
    SetOnSendAudioToSpeechRecognitionCallback(
        base::BindLambdaForTesting([&](media::mojom::AudioDataS16Ptr buffer) {
          EXPECT_EQ(sample_rate, buffer->sample_rate);
          EXPECT_EQ(frame_count, buffer->frame_count);

          run_loop.Quit();
        }));
    if (stop) {
      audio_source_fetcher()->Stop();
    }
    run_loop.Run();
  }

  // media::mojom::SpeechRecognitionRecognizerClient:
  // Left these function do nothing since we are not interested in this test.
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override {}
  void OnSpeechRecognitionStopped() override {}
  void OnSpeechRecognitionError() override {}
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {}

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AudioSourceFetcherImpl> audio_source_fetcher_;
  base::HistogramTester histogram_tester_;

 private:
  raw_ptr<MockAudioSourceConsumer, DanglingUntriaged>
      speech_recognition_recognizer_;
  bool is_server_based_;
};

TEST_P(AudioSourceFetcherImplTest, Resample) {
  MockStreamFactory fake_stream_factory;
  media::AudioParameters params =
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Stereo(),
                             /*sample_rate=*/kOriginalSampleRate,
                             /*frames_per_buffer=*/kOriginalFramesPerBuffer);
  audio_source_fetcher()->Start(fake_stream_factory.MakeRemote(), "device_id",
                                params);

  std::unique_ptr<::media::AudioBus> audio_bus =
      media::AudioBus::Create(params);

  // Initialize channel data in `audio_bus`.
  audio_bus->Zero();
  audio_source_fetcher()->Capture(audio_bus.get(),
                                  /*audio_capture_time=*/base::TimeTicks::Now(),
                                  /*glitch_info=*/{},
                                  /*volume=*/1.0,
                                  /*key_pressed=*/true);
  if (is_server_based()) {
    VerifyAudioBuffer(kServerBasedRecognitionAudioSampleRate,
                      kServerBasedRecognitionAudioFramesPerBuffer);
  } else {
    VerifyAudioBuffer(kOriginalSampleRate, kOriginalFramesPerBuffer);
  }

  fake_stream_factory.ResetReceiver();
  audio_source_fetcher()->Stop();

  // There are remaining frames which are flushed to convert when calling
  // `audio_source_fetcher()->Stop()`.
  if (is_server_based()) {
    VerifyAudioBuffer(kServerBasedRecognitionAudioSampleRate,
                      kServerBasedRecognitionAudioFramesPerBuffer);
  }

  // Let's destroy the audio source fetcher and ensure that the metric
  // has been recorded.
  audio_source_fetcher_.reset();
  base::TimeDelta length = media::AudioTimestampHelper::FramesToTime(
      audio_bus->frames(), kOriginalSampleRate);

  const auto* histogram_name = is_server_based()
                                   ? kServerBasedRecognitionSessionLength
                                   : kOnDeviceRecognitionSessionLength;
  histogram_tester_.ExpectTimeBucketCount(histogram_name, length,
                                          /*count=*/1);
}

TEST_P(AudioSourceFetcherImplTest, StopDuringResample) {
  MockStreamFactory fake_stream_factory;
  media::AudioParameters params =
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Stereo(),
                             /*sample_rate=*/kOriginalSampleRate,
                             /*frames_per_buffer=*/kOriginalFramesPerBuffer);
  audio_source_fetcher()->Start(fake_stream_factory.MakeRemote(), "device_id",
                                params);

  auto audio_bus = media::AudioBus::Create(params);

  // Initialize channel data in `audio_bus`.
  audio_bus->Zero();
  audio_source_fetcher()->Capture(audio_bus.get(),
                                  /*audio_capture_time=*/base::TimeTicks::Now(),
                                  /*glitch_info=*/{},
                                  /*volume=*/1.0,
                                  /*key_pressed=*/true);
  if (is_server_based()) {
    // Stop will prevent the pending resample call from running, so no audio
    // will be available to verify.
    audio_source_fetcher_->Stop();
    task_environment_.RunUntilIdle();
  } else {
    VerifyAudioBuffer(kOriginalSampleRate, kOriginalFramesPerBuffer,
                      /*stop=*/true);
  }
  audio_source_fetcher_.reset();
  fake_stream_factory.ResetReceiver();
}

INSTANTIATE_TEST_SUITE_P(All, AudioSourceFetcherImplTest, ::testing::Bool());

}  // namespace speech
