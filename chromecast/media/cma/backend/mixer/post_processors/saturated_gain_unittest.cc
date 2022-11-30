// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "chromecast/media/cma/backend/mixer/post_processors/post_processor_unittest.h"
#include "chromecast/media/cma/backend/mixer/post_processors/saturated_gain.h"

namespace chromecast {
namespace media {
namespace post_processor_test {

namespace {

const char kConfigTemplate[] = R"config({"gain_db": %f})config";

std::string MakeConfigString(float gain_db) {
  return base::StringPrintf(kConfigTemplate, gain_db);
}

}  // namespace

// Default tests from post_processor_test
TEST_P(PostProcessorTest, SaturatedGainDelay) {
  std::string config = MakeConfigString(0.0);
  auto pp = std::make_unique<SaturatedGain>(config, kNumChannels);
  TestDelay(pp.get(), sample_rate_);
}

TEST_P(PostProcessorTest, SaturatedGainRinging) {
  std::string config = MakeConfigString(0.0);
  auto pp = std::make_unique<SaturatedGain>(config, kNumChannels);
  TestRingingTime(pp.get(), sample_rate_);
}

// Also tests clipping (by attempting to set gain way too high).
TEST_P(PostProcessorTest, SaturatedGainPassthrough) {
  std::string config = MakeConfigString(100.0);
  auto pp = std::make_unique<SaturatedGain>(config, kNumChannels);
  TestPassthrough(pp.get(), sample_rate_);
}

TEST_P(PostProcessorTest, Gain) {
  const int kNumFrames = 256;
  std::string config = MakeConfigString(20.0);  // Exactly 10x multiplier.
  auto pp = std::make_unique<SaturatedGain>(config, kNumChannels);
  auto data = LinearChirp(kNumFrames, std::vector<double>(kNumChannels, 0.0),
                          std::vector<double>(kNumChannels, 1.0));

  for (size_t i = 0; i < data.size(); ++i) {
    data[i] /= 100.0;
  }
  float original_amplitude =
      SineAmplitude(data.data(), kNumChannels * kNumFrames);
  AudioPostProcessor2::Metadata metadata = {-20.0, -20.0, 1.0};
  pp->ProcessFrames(data.data(), kNumFrames, &metadata);

  EXPECT_FLOAT_EQ(original_amplitude * 10.0,
                  SineAmplitude(data.data(), kNumChannels * kNumFrames))
      << "Expected a gain of 20dB";
}

TEST_P(PostProcessorTest, SaturatedGainBenchmark) {
  std::string config = MakeConfigString(20.0);
  auto pp = std::make_unique<SaturatedGain>(config, kNumChannels);
  AudioProcessorBenchmark(pp.get(), sample_rate_);
}

}  // namespace post_processor_test
}  // namespace media
}  // namespace chromecast

/*
Benchmark results:
Device: Google Home Max, test audio duration: 1 sec.
Benchmark                Sample Rate    CPU(%)
----------------------------------------------------
SaturatedGainBenchmark   44100          0.0014%
SaturatedGainBenchmark   48000          0.0016%
*/
