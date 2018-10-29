// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/filter_group.h"

#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "chromecast/media/cma/backend/mixer_input.h"
#include "chromecast/media/cma/backend/mock_mixer_source.h"
#include "chromecast/media/cma/backend/post_processing_pipeline.h"
#include "chromecast/media/cma/backend/stream_mixer.h"
#include "chromecast/public/volume_control.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

using ::testing::_;
using ::testing::NiceMock;

namespace {

// Total of Test samples including left and right channels.
#define NUM_SAMPLES 64

constexpr size_t kBytesPerSample = sizeof(int32_t);
constexpr int kNumInputChannels = 2;
constexpr int kInputSampleRate = 48000;
constexpr int kInputFrames = NUM_SAMPLES / 2;

constexpr AudioContentType kDefaultContentType = AudioContentType::kMedia;
constexpr int kDefaultPlayoutChannel = -1;

class MockPostProcessingPipeline : public PostProcessingPipeline {
 public:
  MockPostProcessingPipeline()
      : MockPostProcessingPipeline(kNumInputChannels) {}

  MockPostProcessingPipeline(int num_output_channels)
      : num_output_channels_(num_output_channels) {
    ON_CALL(*this, ProcessFrames(_, _, _, _))
        .WillByDefault(
            testing::Invoke(this, &MockPostProcessingPipeline::StorePtr));
  }

  ~MockPostProcessingPipeline() override {}
  MOCK_METHOD4(
      ProcessFrames,
      int(float* data, int num_frames, float current_volume, bool is_silence));
  MOCK_METHOD2(SetPostProcessorConfig,
               void(const std::string& name, const std::string& config));
  MOCK_METHOD1(SetContentType, void(AudioContentType));
  MOCK_METHOD1(UpdatePlayoutChannel, void(int));

 protected:
  float* output_buffer_;

 private:
  bool SetSampleRate(int sample_rate) override { return true; }
  bool IsRinging() override { return false; }
  float* GetOutputBuffer() override { return output_buffer_; }
  int NumOutputChannels() override { return num_output_channels_; }
  int delay() { return 0; }
  std::string name() const { return "mock"; }
  int StorePtr(float* data,
               int num_frames,
               float current_volume,
               bool is_silence) {
    output_buffer_ = data;
    return 0;
  }

  const int num_output_channels_;

  DISALLOW_COPY_AND_ASSIGN(MockPostProcessingPipeline);
};

// PostProcessor that inverts one channel.
class InvertChannelPostProcessor : public MockPostProcessingPipeline {
 public:
  explicit InvertChannelPostProcessor(int channels, int channel_to_invert)
      : channels_(channels), channel_to_invert_(channel_to_invert) {
    ON_CALL(*this, ProcessFrames(_, _, _, _))
        .WillByDefault(testing::Invoke(
            this, &InvertChannelPostProcessor::DoInvertChannel));
  }

  ~InvertChannelPostProcessor() override {}

  MOCK_METHOD4(
      ProcessFrames,
      int(float* data, int num_frames, float current_volume, bool is_silence));
  MOCK_METHOD2(SetPostProcessorConfig,
               void(const std::string& name, const std::string& config));

 private:
  int DoInvertChannel(float* data,
                      int num_frames,
                      float current_volume,
                      bool is_silence) {
    output_buffer_ = data;
    for (int fr = 0; fr < num_frames; ++fr) {
      for (int ch = 0; ch < channels_; ++ch) {
        if (ch == channel_to_invert_) {
          data[fr * channels_ + ch] *= -1;
        }
      }
    }
    return 0;
  }

  bool SetSampleRate(int sample_rate) override { return true; }
  bool IsRinging() override { return false; }
  int delay() { return 0; }
  std::string name() const { return "invert"; }

  int channels_;
  int channel_to_invert_;

  DISALLOW_COPY_AND_ASSIGN(InvertChannelPostProcessor);
};

}  // namespace

// Note: Test data should be represented as 32-bit integers and copied into
// ::media::AudioBus instances, rather than wrapping statically declared float
// arrays.
constexpr int32_t kTestData[NUM_SAMPLES] = {
    74343736,    -1333200799, -1360871126, 1138806283,  1931811865,
    1856308487,  649203634,   564640023,   1676630678,  23416591,
    -1293255456, 547928305,   -976258952,  1840550252,  1714525174,
    358704931,   983646295,   1264863573,  442473973,   1222979052,
    317404525,   366912613,   1393280948,  -1022004648, -2054669405,
    -159762261,  1127018745,  -1984491787, 1406988336,  -693327981,
    -1549544744, 1232236854,  970338338,   -1750160519, -783213057,
    1231504562,  1155296810,  -820018779,  1155689800,  -1108462340,
    -150535168,  1033717023,  2121241397,  1829995370,  -1893006836,
    -819097508,  -495186107,  1001768909,  -1441111852, 692174781,
    1916569026,  -687787473,  -910565280,  1695751872,  994166817,
    1775451433,  909418522,   492671403,   -761744663,  -2064315902,
    1357716471,  -1580019684, 1872702377,  -1524457840,
};

// Return a scoped pointer filled with the data above.
std::unique_ptr<::media::AudioBus> GetTestData() {
  int samples = NUM_SAMPLES / kNumInputChannels;
  auto data = ::media::AudioBus::Create(kNumInputChannels, samples);
  data->FromInterleaved(kTestData, samples, kBytesPerSample);
  return data;
}

class FilterGroupTest : public testing::Test {
 protected:
  using RenderingDelay = MixerInput::RenderingDelay;
  FilterGroupTest()
      : source_(kInputSampleRate),
        input_(&source_, kInputSampleRate, 0, RenderingDelay(), nullptr) {
    source_.SetData(GetTestData());
  }

  ~FilterGroupTest() override {}

  void MakeFilterGroup(
      FilterGroup::GroupType type,
      bool mix_to_mono,
      std::unique_ptr<MockPostProcessingPipeline> post_processor) {
    post_processor_ = post_processor.get();
    EXPECT_CALL(*post_processor_, SetContentType(kDefaultContentType));
    EXPECT_CALL(*post_processor_, UpdatePlayoutChannel(kDefaultPlayoutChannel));
    filter_group_ = std::make_unique<FilterGroup>(
        kNumInputChannels, type, "test_filter", std::move(post_processor),
        base::flat_set<std::string>() /* device_ids */,
        std::vector<FilterGroup*>());
    filter_group_->SetMixToMono(mix_to_mono);
    filter_group_->Initialize(kInputSampleRate);
    filter_group_->AddInput(&input_);
    filter_group_->UpdatePlayoutChannel(kChannelAll);
  }

  float Input(int channel, int frame) {
    DCHECK_LE(channel, source_.data().channels());
    DCHECK_LE(frame, source_.data().frames());
    return source_.data().channel(channel)[frame];
  }

  void AssertPassthrough() {
    // Verify if the fiter group output matches the source.
    float* interleaved_data = filter_group_->GetOutputBuffer();
    for (int f = 0; f < kInputFrames; ++f) {
      for (int ch = 0; ch < kNumInputChannels; ++ch) {
        ASSERT_EQ(Input(ch, f), interleaved_data[f * kNumInputChannels + ch])
            << f;
      }
    }
  }

  float LeftInput(int frame) { return Input(0, frame); }
  float RightInput(int frame) { return Input(1, frame); }

  NiceMock<MockMixerSource> source_;
  MixerInput input_;
  std::unique_ptr<FilterGroup> filter_group_;
  MockPostProcessingPipeline* post_processor_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(FilterGroupTest);
};

TEST_F(FilterGroupTest, Passthrough) {
  MakeFilterGroup(FilterGroup::GroupType::kFinalMix, false /* mix to mono */,
                  std::make_unique<NiceMock<MockPostProcessingPipeline>>());
  EXPECT_CALL(*post_processor_, ProcessFrames(_, kInputFrames, _, false));

  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
  AssertPassthrough();
}

TEST_F(FilterGroupTest, StreamGroupsDoNotMonoMix) {
  MakeFilterGroup(FilterGroup::GroupType::kStream, true /* mix to mono */,
                  std::make_unique<NiceMock<MockPostProcessingPipeline>>());
  EXPECT_CALL(*post_processor_, ProcessFrames(_, kInputFrames, _, false));

  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
  AssertPassthrough();
}

TEST_F(FilterGroupTest, LinearizeGroupsDoNotMonoMix) {
  MakeFilterGroup(FilterGroup::GroupType::kLinearize, true /* mix to mono */,
                  std::make_unique<NiceMock<MockPostProcessingPipeline>>());
  EXPECT_CALL(*post_processor_, ProcessFrames(_, kInputFrames, _, false));

  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
  AssertPassthrough();
}

TEST_F(FilterGroupTest, MonoMixer) {
  MakeFilterGroup(FilterGroup::GroupType::kFinalMix, true /* mix to mono */,
                  std::make_unique<NiceMock<MockPostProcessingPipeline>>());
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());

  // Verify if the fiter group output matches the source after down mixing.
  float* interleaved_data = filter_group_->GetOutputBuffer();
  for (int i = 0; i < kInputFrames; ++i) {
    ASSERT_EQ((LeftInput(i) + RightInput(i)) / 2, interleaved_data[i * 2]);
    ASSERT_EQ(interleaved_data[i * 2], interleaved_data[i * 2 + 1]);
  }
}

TEST_F(FilterGroupTest, MonoMixesAfterPostProcessors) {
  MakeFilterGroup(FilterGroup::GroupType::kFinalMix, true /* mix to mono */,
                  std::make_unique<NiceMock<InvertChannelPostProcessor>>(
                      kNumInputChannels, 0));
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());

  // Verify both output channels = (-1 * ch0 + ch1) / 2 after mixing.
  // If order of mixing, filtering is incorrect, the channels won't match.
  float* interleaved_data = filter_group_->GetOutputBuffer();
  for (int i = 0; i < kInputFrames; ++i) {
    ASSERT_EQ((-LeftInput(i) + RightInput(i)) / 2, interleaved_data[i * 2]);
    ASSERT_EQ(interleaved_data[i * 2], interleaved_data[i * 2 + 1]);
  }
}

TEST_F(FilterGroupTest, StreamGroupDoesNotSelectChannels) {
  MakeFilterGroup(FilterGroup::GroupType::kStream, false /* mix to mono */,
                  std::make_unique<NiceMock<MockPostProcessingPipeline>>());

  EXPECT_CALL(*post_processor_, UpdatePlayoutChannel(0));
  filter_group_->UpdatePlayoutChannel(0);
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
  AssertPassthrough();

  source_.SetData(GetTestData());
  EXPECT_CALL(*post_processor_, UpdatePlayoutChannel(1));
  filter_group_->UpdatePlayoutChannel(1);
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
  AssertPassthrough();
}

TEST_F(FilterGroupTest, MixGroupDoesNotSelectChannels) {
  MakeFilterGroup(FilterGroup::GroupType::kFinalMix, false /* mix to mono */,
                  std::make_unique<NiceMock<MockPostProcessingPipeline>>());

  EXPECT_CALL(*post_processor_, UpdatePlayoutChannel(0));
  filter_group_->UpdatePlayoutChannel(0);
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
  AssertPassthrough();

  source_.SetData(GetTestData());
  EXPECT_CALL(*post_processor_, UpdatePlayoutChannel(1));
  filter_group_->UpdatePlayoutChannel(1);
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
  AssertPassthrough();
}

TEST_F(FilterGroupTest, SelectsOutputChannel) {
  MakeFilterGroup(FilterGroup::GroupType::kLinearize, false /* mix to mono */,
                  std::make_unique<NiceMock<MockPostProcessingPipeline>>());

  EXPECT_CALL(*post_processor_, UpdatePlayoutChannel(0));
  filter_group_->UpdatePlayoutChannel(0);
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());

  float* interleaved_data = filter_group_->GetOutputBuffer();
  for (int f = 0; f < kInputFrames; ++f) {
    for (int ch = 0; ch < kNumInputChannels; ++ch) {
      // Both output channels should be equal to left channel.
      ASSERT_EQ(interleaved_data[f * kNumInputChannels + ch], LeftInput(f));
    }
  }

  testing::Mock::VerifyAndClearExpectations(post_processor_);
  source_.SetData(GetTestData());

  EXPECT_CALL(*post_processor_, UpdatePlayoutChannel(1));
  filter_group_->UpdatePlayoutChannel(1);
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
  for (int f = 0; f < kInputFrames; ++f) {
    for (int ch = 0; ch < kNumInputChannels; ++ch) {
      // Both output channels should be equal to right channel.
      ASSERT_EQ(interleaved_data[f * kNumInputChannels + ch], RightInput(f));
    }
  }

  testing::Mock::VerifyAndClearExpectations(post_processor_);
  source_.SetData(GetTestData());

  EXPECT_CALL(*post_processor_, UpdatePlayoutChannel(-1));
  filter_group_->UpdatePlayoutChannel(-1);
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
  for (int f = 0; f < kInputFrames; ++f) {
    for (int ch = 0; ch < kNumInputChannels; ++ch) {
      // Back to normal (passthrough).
      ASSERT_EQ(interleaved_data[f * kNumInputChannels + ch], Input(ch, f));
    }
  }
}

TEST_F(FilterGroupTest, SelectsOutputChannelBeforePostProcessors) {
  MakeFilterGroup(FilterGroup::GroupType::kLinearize, false /* mix to mono */,
                  std::make_unique<NiceMock<InvertChannelPostProcessor>>(
                      kNumInputChannels, 0));
  EXPECT_CALL(*post_processor_, UpdatePlayoutChannel(0));
  filter_group_->UpdatePlayoutChannel(0);
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());

  float* interleaved_data = filter_group_->GetOutputBuffer();
  for (int f = 0; f < kInputFrames; ++f) {
    // channel 0 out = channel 0 in * -1
    // channel 1 out = channel 0 in
    // (If order is wrong, both channels will be channel_0_in * -1).
    ASSERT_EQ(interleaved_data[f * kNumInputChannels], LeftInput(f) * -1);
    ASSERT_EQ(interleaved_data[f * kNumInputChannels + 1], LeftInput(f));
  }
}

TEST_F(FilterGroupTest, ChecksContentType) {
  MakeFilterGroup(FilterGroup::GroupType::kStream, false,
                  std::make_unique<NiceMock<MockPostProcessingPipeline>>());

  NiceMock<MockMixerSource> tts_source(kInputSampleRate);
  tts_source.set_content_type(AudioContentType::kCommunication);
  MixerInput tts_input(&tts_source, kInputSampleRate, 0, RenderingDelay(),
                       nullptr);

  NiceMock<MockMixerSource> alarm_source(kInputSampleRate);
  alarm_source.set_content_type(AudioContentType::kAlarm);
  MixerInput alarm_input(&alarm_source, kInputSampleRate, 0, RenderingDelay(),
                         nullptr);

  // Media input stream + tts input stream -> tts content type.
  filter_group_->AddInput(&tts_input);
  EXPECT_CALL(*post_processor_,
              SetContentType(AudioContentType::kCommunication));
  tts_source.SetData(GetTestData());
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());

  // Media input + tts input + alarm input -> tts content type (no update).
  filter_group_->AddInput(&alarm_input);
  EXPECT_CALL(*post_processor_,
              SetContentType(AudioContentType::kCommunication))
      .Times(0);
  source_.SetData(GetTestData());
  tts_source.SetData(GetTestData());
  alarm_source.SetData(GetTestData());
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());

  // Media input + alarm input -> alarm content type.
  filter_group_->RemoveInput(&tts_input);
  EXPECT_CALL(*post_processor_, SetContentType(AudioContentType::kAlarm));
  source_.SetData(GetTestData());
  alarm_source.SetData(GetTestData());
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());

  // Media input stream -> media input
  EXPECT_CALL(*post_processor_, SetContentType(AudioContentType::kMedia));
  filter_group_->RemoveInput(&alarm_input);
  source_.SetData(GetTestData());
  filter_group_->MixAndFilter(kInputFrames, RenderingDelay());
}

TEST_F(FilterGroupTest, ReportsOutputChannels) {
  const int num_output_channels = 4;
  MakeFilterGroup(FilterGroup::GroupType::kStream, false,
                  std::make_unique<NiceMock<MockPostProcessingPipeline>>(
                      num_output_channels));

  EXPECT_EQ(num_output_channels, filter_group_->GetOutputChannelCount());
}

}  // namespace media
}  // namespace chromecast
