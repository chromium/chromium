// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "base/check.h"
#include "chromecast/media/base/slew_volume.h"
#include "media/base/audio_bus.h"
#include "media/base/vector_math.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

const int kNumChannels = 2;
const int kNumFrames = 100;
const float kSinFrequency = 1.0f / kNumFrames;
const int kBytesPerSample = sizeof(int32_t);

// Frequency is in frames (frequency = frequency_in_hz / sample rate).
std::unique_ptr<::media::AudioBus> GetSineData(size_t frames, float frequency) {
  auto data = ::media::AudioBus::Create(kNumChannels, frames);
  std::vector<int32_t> sine(frames * 2);
  for (size_t i = 0; i < frames; ++i) {
    // Offset by 1 because sin(0) = 0 and the first value is a special case.
    sine[i * 2] = sin(static_cast<float>(i + 1) * frequency * 2 * M_PI) *
                  std::numeric_limits<int32_t>::max();
    sine[i * 2 + 1] = cos(static_cast<float>(i + 1) * frequency * 2 * M_PI) *
                      std::numeric_limits<int32_t>::max();
  }
  data->FromInterleaved<::media::SignedInt32SampleTypeTraits>(sine.data(),
                                                              frames);
  return data;
}

// Gets pointers to the data in an audiobus.
// If |swapped| is true, the channel order will be swapped.
std::vector<float*> GetDataChannels(::media::AudioBus* audio,
                                    bool swapped = false) {
  std::vector<float*> data(kNumChannels);
  for (int i = 0; i < kNumChannels; ++i) {
    int source_channel = swapped ? (i + 1) % kNumChannels : i;
    data[i] = audio->channel(source_channel);
  }
  return data;
}

void ScaleData(const std::vector<float*>& data, int frames, float scale) {
  for (size_t ch = 0; ch < data.size(); ++ch) {
    for (int f = 0; f < frames; ++f) {
      data[ch][f] *= scale;
    }
  }
}

void CompareDataPartial(const std::vector<float*>& expected,
                        const std::vector<float*>& actual,
                        int start,
                        int end) {
  ASSERT_GE(start, 0);
  ASSERT_LT(start, end);
  ASSERT_EQ(expected.size(), actual.size());

  for (size_t ch = 0; ch < expected.size(); ++ch) {
    for (int f = start; f < end; ++f) {
      EXPECT_FLOAT_EQ(expected[ch][f], actual[ch][f])
          << "ch: " << ch << " f: " << f;
    }
  }
}

}  // namespace

class SlewVolumeBaseTest : public ::testing::Test {
 public:
  SlewVolumeBaseTest(const SlewVolumeBaseTest&) = delete;
  SlewVolumeBaseTest& operator=(const SlewVolumeBaseTest&) = delete;

 protected:
  SlewVolumeBaseTest() = default;
  ~SlewVolumeBaseTest() override = default;

  void SetUp() override {
    slew_volume_ = std::make_unique<SlewVolume>();
    slew_volume_->Interrupted();
    MakeData(kNumFrames);
  }

  void MakeData(int num_frames) {
    num_frames_ = num_frames;
    data_bus_ = GetSineData(num_frames_, kSinFrequency);
    data_bus_2_ = GetSineData(num_frames_, kSinFrequency);
    expected_bus_ = GetSineData(num_frames_, kSinFrequency);
    data_ = GetDataChannels(data_bus_.get());
    data_2_ = GetDataChannels(data_bus_2_.get(), true /* swapped */);
    expected_ = GetDataChannels(expected_bus_.get());
  }

  void CompareBuffers(int start = 0, int end = -1) {
    if (end == -1) {
      end = num_frames_;
    }

    ASSERT_GE(start, 0);
    ASSERT_LT(start, end);
    ASSERT_LE(end, num_frames_);

    CompareDataPartial(expected_, data_, start, end);
  }

  void ClearInterrupted() {
    float throwaway __attribute__((__aligned__(16))) = 0.0f;
    slew_volume_->ProcessFMUL(false, &throwaway, 1, 1, &throwaway);
  }

  int num_frames_;

  std::unique_ptr<SlewVolume> slew_volume_;
  std::unique_ptr<::media::AudioBus> data_bus_;
  std::unique_ptr<::media::AudioBus> data_bus_2_;
  std::unique_ptr<::media::AudioBus> expected_bus_;
  std::vector<float*> data_;
  std::vector<float*> data_2_;
  std::vector<float*> expected_;
};

// ASSERT_DEATH isn't implemented on Fuchsia.
#if defined(ASSERT_DEATH)

TEST_F(SlewVolumeBaseTest, BadSampleRate) {
// String arguments aren't passed to CHECK() in official builds.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  ASSERT_DEATH(slew_volume_->SetSampleRate(0), "");
#else
  ASSERT_DEATH(slew_volume_->SetSampleRate(0), "sample_rate");
#endif
}

TEST_F(SlewVolumeBaseTest, BadSlewTime) {
  ASSERT_DEATH(slew_volume_->SetMaxSlewTimeMs(-1), "");
}

#endif  // defined(ASSERT_DEATH)

TEST_F(SlewVolumeBaseTest, InstantVolumeDecreasing) {
  slew_volume_->SetMaxSlewTimeMs(10);
  slew_volume_->SetSampleRate(10000);
  // Max slew per sample = 1000 / (max_time * sample_rate)
  //                     = 0.01
  slew_volume_->SetVolume(1.0);
  ClearInterrupted();
  slew_volume_->SetVolume(0.0);
  const int kFramesPerTransaction = 10;
  // LastVolume lags, so 101 steps are needed.
  for (int i = 0; i < 101; i += kFramesPerTransaction) {
    for (size_t ch = 0; ch < data_.size(); ++ch) {
      slew_volume_->ProcessFMAC(ch != 0, data_[ch], 10, 1, data_2_[ch]);
    }
    ASSERT_FLOAT_EQ(1.0 - (0.01 * i), slew_volume_->LastBufferMaxMultiplier());
  }
}

TEST_F(SlewVolumeBaseTest, InstantVolumeIncreasing) {
  slew_volume_->SetMaxSlewTimeMs(10);
  slew_volume_->SetSampleRate(10000);
  // Max slew per sample = 1000 / (max_time * sample_rate)
  //                     = 0.01
  slew_volume_->SetVolume(0.0);
  ClearInterrupted();
  slew_volume_->SetVolume(1.0);
  const int kFramesPerTransaction = 10;
  // LastVolume leads, so 100 steps are needed.
  for (int i = 0; i < 100; i += kFramesPerTransaction) {
    for (size_t ch = 0; ch < data_.size(); ++ch) {
      slew_volume_->ProcessFMAC(ch != 0, data_[ch], 10, 1, data_2_[ch]);
    }
    ASSERT_FLOAT_EQ(0.01 * (i + kFramesPerTransaction),
                    slew_volume_->LastBufferMaxMultiplier());
  }
}

class SlewVolumeSteadyStateTest : public SlewVolumeBaseTest {
 public:
  SlewVolumeSteadyStateTest(const SlewVolumeSteadyStateTest&) = delete;
  SlewVolumeSteadyStateTest& operator=(const SlewVolumeSteadyStateTest&) =
      delete;

 protected:
  SlewVolumeSteadyStateTest() = default;
  ~SlewVolumeSteadyStateTest() override = default;

  void SetUp() override {
    SlewVolumeBaseTest::SetUp();
    slew_volume_->Interrupted();
  }
};

TEST_F(SlewVolumeSteadyStateTest, FMULNoOp) {
  slew_volume_->SetVolume(1.0f);

  slew_volume_->ProcessFMUL(false /* repeat transition */, data_[0],
                            num_frames_, 1, data_[0]);
  slew_volume_->ProcessFMUL(true /* repeat transition */, data_[1], num_frames_,
                            1, data_[1]);
  CompareBuffers();
}

TEST_F(SlewVolumeSteadyStateTest, FMULCopy) {
  slew_volume_->SetVolume(1.0f);

  slew_volume_->ProcessFMUL(false /* repeat transition */, data_2_[0],
                            num_frames_, 1, data_[0]);
  slew_volume_->ProcessFMUL(true /* repeat transition */, data_2_[1],
                            num_frames_, 1, data_[1]);
  CompareDataPartial(data_2_, data_, 0, num_frames_);
}

TEST_F(SlewVolumeSteadyStateTest, FMULZero) {
  slew_volume_->SetVolume(0.0f);
  slew_volume_->ProcessFMUL(false, /* repeat transition */
                            data_[0], num_frames_, 1, data_[0]);
  slew_volume_->ProcessFMUL(true, data_[1], num_frames_, 1, data_[1]);

  for (size_t ch = 0; ch < data_.size(); ++ch) {
    for (int f = 0; f < num_frames_; ++f) {
      EXPECT_EQ(0.0f, data_[ch][f]) << "at ch " << ch << "frame " << f;
    }
  }
}

TEST_F(SlewVolumeSteadyStateTest, FMULInterrupted) {
  float volume = 0.6f;
  slew_volume_->SetVolume(volume);

  slew_volume_->ProcessFMUL(false, data_[0], num_frames_, 1, data_[0]);
  slew_volume_->ProcessFMUL(true, data_[1], num_frames_, 1, data_[1]);
  ScaleData(expected_, num_frames_, volume);
  CompareBuffers();
}

TEST_F(SlewVolumeSteadyStateTest, FMACNoOp) {
  slew_volume_->SetVolume(0.0f);
  slew_volume_->ProcessFMAC(false, data_2_[0], num_frames_, 1, data_[0]);
  slew_volume_->ProcessFMAC(false, data_2_[1], num_frames_, 1, data_[1]);
  CompareBuffers();
}

class SlewVolumeDynamicTest
    : public SlewVolumeBaseTest,
      public ::testing::WithParamInterface<std::tuple<int, int>> {
 public:
  SlewVolumeDynamicTest(const SlewVolumeDynamicTest&) = delete;
  SlewVolumeDynamicTest& operator=(const SlewVolumeDynamicTest&) = delete;

 protected:
  SlewVolumeDynamicTest() = default;
  ~SlewVolumeDynamicTest() override = default;

  void SetUp() override {
    SlewVolumeBaseTest::SetUp();
    channels_ = 2;
    sample_rate_ = std::get<0>(GetParam());
    slew_time_ms_ = std::get<1>(GetParam());
    slew_time_frames_ = sample_rate_ * slew_time_ms_ / 1000;
    slew_volume_->SetSampleRate(sample_rate_);
    slew_volume_->SetMaxSlewTimeMs(slew_time_ms_);
    // +2 frames for numeric errors.
    int num_frames = slew_time_frames_ + 2;
    max_frame_ = num_frames - 1;
    ASSERT_GE(num_frames, 1);
    MakeData(num_frames);
  }

  // Checks data_ = slew_volume_(expected_).
  void CheckSlewMUL(double start_vol, double end_vol) {
    for (size_t ch = 0; ch < data_.size(); ++ch) {
      // First value should have original scaling applied.
      EXPECT_FLOAT_EQ(Expected(ch, 0) * start_vol, Data(ch, 0)) << ch;
      for (int f = 1; f < slew_time_frames_; ++f) {
        // Can't calculate gain if input is 0.
        if (Expected(ch, f) == 0.0)
          continue;
        double actual_gain = Data(ch, f) / Expected(ch, f);
        // Interpolate to get expected gain.
        double frame_gain_change = (end_vol - start_vol) / (slew_time_frames_);
        double expected_gain = frame_gain_change * f + start_vol;
        EXPECT_LE(std::abs(actual_gain - expected_gain),
                  std::abs(frame_gain_change))
            << ch << " " << f;
      }
      // Steady state should have final scaling applied.
      int f = max_frame_;
      EXPECT_FLOAT_EQ(Expected(ch, f) * end_vol, Data(ch, f))
          << ch << " " << f
          << " Actual gain = " << Data(ch, f) / Expected(ch, f);
    }
  }

  // Checks data_ = expected_ + slew_volume_(data_2_).
  void CheckSlewMAC(double start_vol, double end_vol) {
    for (int ch = 0; ch < channels_; ++ch) {
      // First value should have original scaling applied.
      EXPECT_FLOAT_EQ(Expected(ch, 0) + Data2(ch, 0) * start_vol, Data(ch, 0))
          << ch;
      for (int f = 1; f < slew_time_frames_; ++f) {
        // Can't calculate gain if input is 0.
        if (Data2(ch, f) == 0.0)
          continue;
        double actual_gain = (Data(ch, f) - Expected(ch, f)) / Data2(ch, f);
        // Interpolate to get expected gain.
        double frame_gain_change = (end_vol - start_vol) / (slew_time_frames_);
        double expected_gain = frame_gain_change * f + start_vol;
        EXPECT_LE(std::abs(actual_gain - expected_gain),
                  std::abs(frame_gain_change))
            << f;
      }
      // Steady state should have final gain applied.
      int f = max_frame_;
      EXPECT_FLOAT_EQ(Expected(ch, f) + Data2(ch, f) * end_vol, Data(ch, f))
          << ch << " " << f << " Actual gain = "
          << (Data(ch, f) - Expected(ch, f)) / Data2(ch, f);
    }
  }

  // Data accessors. Override to change access method of CheckSlewM[AC/UL].
  virtual float Data(int channel, int frame) { return data_[channel][frame]; }

  virtual float Data2(int channel, int frame) {
    return data_2_[channel][frame];
  }

  virtual float Expected(int channel, int frame) {
    return expected_[channel][frame];
  }

  int sample_rate_;
  int slew_time_ms_;
  int slew_time_frames_;
  int channels_;
  int max_frame_;
};

TEST_P(SlewVolumeDynamicTest, FMULRampUp) {
  double start = 0.0;
  double end = 1.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  slew_volume_->ProcessFMUL(false, data_[0], num_frames_, 1, data_[0]);
  slew_volume_->ProcessFMUL(true, data_[1], num_frames_, 1, data_[1]);
  CheckSlewMUL(start, end);
}

TEST_P(SlewVolumeDynamicTest, FMULRampDown) {
  double start = 1.0;
  double end = 0.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  slew_volume_->ProcessFMUL(false, data_[0], num_frames_, 1, data_[0]);
  slew_volume_->ProcessFMUL(true, data_[1], num_frames_, 1, data_[1]);
  CheckSlewMUL(start, end);
}

// Provide data as small buffers.
TEST_P(SlewVolumeDynamicTest, FMULRampDownByParts) {
  double start = 1.0;
  double end = 0.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  int frame_step = ::media::vector_math::kRequiredAlignment / kBytesPerSample;
  int f;
  for (f = 0; f < num_frames_; f += frame_step) {
    // Process any remaining samples in the last step.
    if (num_frames_ - f < frame_step * 2) {
      frame_step = num_frames_ - f;
    }
    slew_volume_->ProcessFMUL(false, expected_[0] + f, frame_step, 1,
                              data_[0] + f);
    slew_volume_->ProcessFMUL(true, expected_[1] + f, frame_step, 1,
                              data_[1] + f);
  }
  ASSERT_EQ(num_frames_, f);
  CheckSlewMUL(start, end);
}

TEST_P(SlewVolumeDynamicTest, FMACRampUp) {
  double start = 0.0;
  double end = 1.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  slew_volume_->ProcessFMAC(false, data_2_[0], num_frames_, 1, data_[0]);
  slew_volume_->ProcessFMAC(true, data_2_[1], num_frames_, 1, data_[1]);
  CheckSlewMAC(start, end);
}

TEST_P(SlewVolumeDynamicTest, FMACRampDown) {
  double start = 1.0;
  double end = 0.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  slew_volume_->ProcessFMAC(false, data_2_[0], num_frames_, 1, data_[0]);
  slew_volume_->ProcessFMAC(true, data_2_[1], num_frames_, 1, data_[1]);
  CheckSlewMAC(start, end);
}

// Provide data as small buffers.
TEST_P(SlewVolumeDynamicTest, FMACRampUpByParts) {
  double start = 0.0;
  double end = 1.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  int frame_step = ::media::vector_math::kRequiredAlignment / kBytesPerSample;
  int f;
  for (f = 0; f < num_frames_; f += frame_step) {
    // Process any remaining samples in the last step.
    if (num_frames_ - f < frame_step * 2) {
      frame_step = num_frames_ - f;
    }
    slew_volume_->ProcessFMAC(false, data_2_[0] + f, frame_step, 1,
                              data_[0] + f);
    slew_volume_->ProcessFMAC(true, data_2_[1] + f, frame_step, 1,
                              data_[1] + f);
  }
  ASSERT_EQ(num_frames_, f);
  CheckSlewMAC(start, end);
}

INSTANTIATE_TEST_SUITE_P(SingleBufferSlew,
                         SlewVolumeDynamicTest,
                         ::testing::Combine(::testing::Values(44100, 48000),
                                            ::testing::Values(0, 15, 100)));

class SlewVolumeInterleavedTest : public SlewVolumeDynamicTest {
 public:
  SlewVolumeInterleavedTest(const SlewVolumeInterleavedTest&) = delete;
  SlewVolumeInterleavedTest& operator=(const SlewVolumeInterleavedTest&) =
      delete;

 protected:
  SlewVolumeInterleavedTest() = default;
  ~SlewVolumeInterleavedTest() override = default;

  void SetUp() override {
    slew_volume_ = std::make_unique<SlewVolume>();
    slew_volume_->Interrupted();

    channels_ = std::get<0>(GetParam());
    sample_rate_ = 16000;
    slew_time_ms_ = 20;
    slew_time_frames_ = sample_rate_ * slew_time_ms_ / 1000;
    slew_volume_->SetMaxSlewTimeMs(slew_time_ms_);
    slew_volume_->SetSampleRate(sample_rate_);
    num_frames_ = (2 + slew_time_frames_) * channels_;
    max_frame_ = num_frames_ / channels_ - 1;
    MakeData(num_frames_);
  }

  float Data(int channel, int frame) override {
    return data_[0][channels_ * frame + channel];
  }

  float Data2(int channel, int frame) override {
    return data_2_[0][channels_ * frame + channel];
  }

  float Expected(int channel, int frame) override {
    return expected_[0][channels_ * frame + channel];
  }
};

TEST_P(SlewVolumeInterleavedTest, FMACRampDown) {
  double start = 1.0;
  double end = 0.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  slew_volume_->ProcessFMAC(false, data_2_[0], num_frames_ / channels_,
                            channels_, data_[0]);
  CheckSlewMAC(start, end);
}

TEST_P(SlewVolumeInterleavedTest, FMACRampUp) {
  double start = 0.0;
  double end = 1.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  slew_volume_->ProcessFMAC(false, data_2_[0], num_frames_ / channels_,
                            channels_, data_[0]);
  CheckSlewMAC(start, end);
}

TEST_P(SlewVolumeInterleavedTest, FMULRampDown) {
  double start = 1.0;
  double end = 0.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  slew_volume_->ProcessFMUL(false, data_[0], num_frames_ / channels_, channels_,
                            data_[0]);

  CheckSlewMUL(start, end);
}

TEST_P(SlewVolumeInterleavedTest, FMULRampUp) {
  double start = 0.0;
  double end = 1.0;
  slew_volume_->SetVolume(start);
  ClearInterrupted();

  slew_volume_->SetVolume(end);
  slew_volume_->ProcessFMUL(false, data_[0], num_frames_ / channels_, channels_,
                            data_[0]);
  CheckSlewMUL(start, end);
}

INSTANTIATE_TEST_SUITE_P(Interleaved,
                         SlewVolumeInterleavedTest,
                         ::testing::Combine(::testing::Values(2, 4),
                                            ::testing::Values(0)));

}  // namespace media
}  // namespace chromecast
