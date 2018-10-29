// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/audio/fake_external_audio_pipeline_support.h"
#include "chromecast/media/cma/backend/mock_mixer_source.h"
#include "chromecast/media/cma/backend/mock_post_processor_factory.h"
#include "chromecast/media/cma/backend/stream_mixer.h"
#include "chromecast/public/media/external_audio_pipeline_shlib.h"
#include "chromecast/public/media/mixer_output_stream.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::_;
using ::testing::AtLeast;

// Mock for saving and checking loopback audio data.
class MockLoopbackAudioObserver : public CastMediaShlib::LoopbackAudioObserver {
 public:
  MockLoopbackAudioObserver() {
    ON_CALL(*this, OnLoopbackAudio(_, _, _, _, _, _))
        .WillByDefault(::testing::Invoke(
            this, &MockLoopbackAudioObserver::OnLoopbackAudioImpl));
  }

  MOCK_METHOD6(OnLoopbackAudio,
               void(int64_t timestamp,
                    SampleFormat sample_format,
                    int sample_rate,
                    int num_channels,
                    uint8_t* data,
                    int length));
  MOCK_METHOD0(OnLoopbackInterrupted, void());
  MOCK_METHOD0(OnRemoved, void());

  const std::vector<float>& data() const { return data_; }

 private:
  void OnLoopbackAudioImpl(int64_t timestamp,
                           SampleFormat sample_format,
                           int sample_rate,
                           int num_channels,
                           uint8_t* data,
                           int length) {
    data_.clear();
    // Save received data to local.
    float* float_data = reinterpret_cast<float*>(const_cast<uint8_t*>(data));
    const size_t size = length / sizeof(float);
    data_.insert(data_.end(), float_data, float_data + size);
  }

  std::vector<float> data_;

  DISALLOW_COPY_AND_ASSIGN(MockLoopbackAudioObserver);
};

class ExternalAudioPipelineTest : public ::testing::Test {
 public:
  ExternalAudioPipelineTest()
      : external_audio_pipeline_support_(
            testing::GetFakeExternalAudioPipelineSupport()),
        message_loop_(std::make_unique<base::MessageLoop>()) {}

  void SetUp() override {
    // Set that external library is supported.
    external_audio_pipeline_support_->SetSupported();

    mixer_ = std::make_unique<StreamMixer>(nullptr, nullptr,
                                           base::ThreadTaskRunnerHandle::Get());
  }

  void TearDown() override {
    // Reset library internal state to use it for other unit tests.
    external_audio_pipeline_support_->Reset();
  }
  // Run async operations in the stream mixer.
  void RunLoopForMixer() {
    // SendLoopbackData.
    base::RunLoop run_loop1;
    message_loop_->task_runner()->PostTask(FROM_HERE, run_loop1.QuitClosure());
    run_loop1.Run();
    // Playbackloop.
    base::RunLoop run_loop2;
    message_loop_->task_runner()->PostTask(FROM_HERE, run_loop2.QuitClosure());
    run_loop2.Run();
  }

 protected:
  std::unique_ptr<StreamMixer> mixer_;
  testing::FakeExternalAudioPipelineSupport* const
      external_audio_pipeline_support_;

 private:
  const std::unique_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ExternalAudioPipelineTest);
};

// Check that |expected| matches |actual| exactly.
void CompareAudioData(const ::media::AudioBus& expected,
                      const ::media::AudioBus& actual) {
  ASSERT_EQ(expected.channels(), actual.channels());
  ASSERT_EQ(expected.frames(), actual.frames());
  for (int c = 0; c < expected.channels(); ++c) {
    const float* expected_data = expected.channel(c);
    const float* actual_data = actual.channel(c);
    for (int f = 0; f < expected.frames(); ++f) {
      EXPECT_FLOAT_EQ(*expected_data++, *actual_data++) << c << " " << f;
    }
  }
}

// Unit tests for ExternalAudioPipelineShlib library.
// Test media volume notification.
TEST_F(ExternalAudioPipelineTest, SetMediaVolume) {
  ASSERT_EQ(external_audio_pipeline_support_->GetVolume(), 0.0f);

  mixer_->SetVolume(AudioContentType::kMedia, 0.02);
  RunLoopForMixer();
  ASSERT_EQ(external_audio_pipeline_support_->GetVolume(), 0.02f);
}
// Test media muted notification.
TEST_F(ExternalAudioPipelineTest, SetMediaMuted) {
  ASSERT_EQ(external_audio_pipeline_support_->IsMuted(), false);

  mixer_->SetMuted(AudioContentType::kMedia, true);
  RunLoopForMixer();
  ASSERT_EQ(external_audio_pipeline_support_->IsMuted(), true);
}
// Set media volume from library, check notification.
TEST_F(ExternalAudioPipelineTest, SetVolumeChangeRequest) {
  ASSERT_EQ(external_audio_pipeline_support_->GetVolume(), 0.0f);

  external_audio_pipeline_support_->OnVolumeChangeRequest(0.03);
  RunLoopForMixer();
  ASSERT_EQ(external_audio_pipeline_support_->GetVolume(), 0.03f);
}
// Set media mute from library, check notification.
TEST_F(ExternalAudioPipelineTest, SetMuteChangeRequest) {
  ASSERT_EQ(external_audio_pipeline_support_->IsMuted(), false);

  external_audio_pipeline_support_->OnMuteChangeRequest(true);
  RunLoopForMixer();
  ASSERT_EQ(external_audio_pipeline_support_->IsMuted(), true);
}
// Check external library loopback data. Check that passed input to StreamMixer
// comes to CastMediaShlib::LoopbackAudioObserver w/o changes.
TEST_F(ExternalAudioPipelineTest, ExternalAudioPipelineLoopbackData) {
  // Set Volume to 1, because we'd like the input to be w/o changes.
  mixer_->SetVolume(AudioContentType::kMedia, 1);

  // Add fake postprocessor to override test configuration running on device.
  mixer_->ResetPostProcessorsForTest(
      std::make_unique<MockPostProcessorFactory>(), "{}");

  // CastMediaShlib::LoopbackAudioObserver mock observer.
  MockLoopbackAudioObserver mock_loopback_observer;
  EXPECT_CALL(mock_loopback_observer, OnLoopbackAudio(_, _, _, _, _, _))
      .Times(AtLeast(1));
  EXPECT_CALL(mock_loopback_observer, OnLoopbackInterrupted())
      .Times(AtLeast(1));
  EXPECT_CALL(mock_loopback_observer, OnRemoved()).Times(1);
  // Input.
  MockMixerSource input(48000);
  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(input, FinalizeAudioPlayback()).Times(1);
  EXPECT_CALL(input, FillAudioPlaybackFrames(_, _, _)).Times(AtLeast(1));

  // Prepare data for test.
  const size_t kSampleSize = 64;
  char test_data[kSampleSize];
  for (size_t i = 0; i < kSampleSize; ++i)
    test_data[i] = i;

  // Set test data in AudioBus.
  const int kNumChannels = 2;
  const auto kNumFrames = kSampleSize / kNumChannels;
  auto data = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  const size_t kBytesPerSample = sizeof(test_data[0]);
  data->FromInterleaved(&test_data, kNumFrames, kBytesPerSample);
  // Prepare data for compare.
  auto expected = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  data->CopyTo(expected.get());

  // Start the test. Set loopback observer.
  mixer_->AddLoopbackAudioObserver(&mock_loopback_observer);

  mixer_->AddInput(&input);

  RunLoopForMixer();
  // Send data to the stream mixer.
  input.SetData(std::move(data));

  RunLoopForMixer();

  // Get actual data from our mocked loopback observer.
  ASSERT_GE(mock_loopback_observer.data().size(), kNumFrames);

  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  using FloatType = ::media::Float32SampleTypeTraits;
  actual->FromInterleaved<FloatType>(mock_loopback_observer.data().data(),
                                     kNumFrames);

  CompareAudioData(*expected, *actual);

  // Check OnRemoved.
  mixer_->RemoveLoopbackAudioObserver(&mock_loopback_observer);
  mixer_->RemoveInput(&input);

  RunLoopForMixer();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
