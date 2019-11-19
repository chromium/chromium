// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_POST_PROCESSOR_UNITTEST_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_POST_PROCESSOR_UNITTEST_H_

#include <string>
#include <vector>

#include "chromecast/media/base/aligned_buffer.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file contains basic tests for AudioPostProcessors.
// All AudioPostProcessors should run (and pass) the following tests:
//  TestDelay (tests return value from ProcessFrames)
//  TestRingingTime (from GetRingingTimeFrames)
// Additionally, if it is possible to configure the PostProcessor to be a
// passthrough (no-op), then you should also run
//  TestPassthrough (tests data in = data out, accounting for delay).
//
// Usage:
// TEST_P(PostProcessorTest, DelayTest) {
//   std::unique_ptr<AudioPostProcessor> my_post_processor(
//       AudioPostProcessorShlib_Create(my_config, 2));
//   TestDelay(my_post_processor, sample_rate_);
// }
// (Repeat for TestRingingTime, TestPassthrough).
//
// This will run your test with 44100 and 48000 sample rates.
// You can also make your own tests using the provided helper functions.

namespace chromecast {
namespace media {
namespace post_processor_test {

const int kBufSizeFrames = 256;
const int kNumChannels = 2;

AudioPostProcessor2::Config MakeProcessorConfig(int sample_rate_hz);

void TestDelay(AudioPostProcessor2* pp,
               int sample_rate,
               int num_input_channels = 2);
void TestRingingTime(AudioPostProcessor2* pp,
                     int sample_rate,
                     int num_input_channels = 2);

// Requires that num_output_channels == |input_channels|
void TestPassthrough(AudioPostProcessor2* pp,
                     int sample_rate,
                     int num_input_channels = 2);

// Legacy tests for AudioPostProcessor(1).
void TestDelay(AudioPostProcessor* pp, int sample_rate);
void TestRingingTime(AudioPostProcessor* pp, int sample_rate);
void TestPassthrough(AudioPostProcessor* pp, int sample_rate);

// Measure amount of CPU time |pp| takes to run [x] seconds of stereo audio at
// |sample_rate|.
void AudioProcessorBenchmark(AudioPostProcessor2* pp,
                             int sample_rate,
                             int num_input_channels = kNumChannels);
void AudioProcessorBenchmark(AudioPostProcessor* pp, int sample_rate);

// Returns the maximum number of frames a PostProcessor may be asked to handle
// in a single call.
int GetMaximumFrames(int sample_rate);

// Tests that the first |size| elements of |expected| and |actual| are the same.
template <typename T>
void CheckArraysEqual(const T* expected, const T* actual, size_t size);

// Returns a list of indexes at which |expected| and |actual| differ.
template <typename T>
std::vector<int> CompareArray(const T* expected, const T* actual, size_t size);

// Print the first |size| elemenents of |array| to a string.
template <typename T>
std::string ArrayToString(const T* array, size_t size);

// Compute the amplitude of a sinusoid as power * sqrt(2)
// This is more robust that looking for the maximum value.
float SineAmplitude(const float* data, int num_samples);

// Return a vector of |frames| frames of |num_channels| interleaved data.
// |frequency| is in hz.
// Each channel, ch,  will be sin(2 *pi * frequency / sample_rate * (n + ch)).
AlignedBuffer<float> GetSineData(int frames,
                                 float frequency,
                                 int sample_rate,
                                 int num_channels = kNumChannels);

// Returns a vector of interleaved chirp waveforms with |frames| frames and
// number of channels equal to |start_frequencies.size()|.
// |start_frequencies| and |end_frequencies| must be the same size.
// Each channel, ch, will have frequency linearly interpolated from
// |start_frequencies[ch]| to |end_frequencies[ch]|
// Frequencies are normalized to (2 * freq_in_hz / sample_rate); 0 = DC, 1 =
// nyquist.
AlignedBuffer<float> LinearChirp(int frames,
                                 const std::vector<double>& start_frequencies,
                                 const std::vector<double>& end_frequencies);

// Returns a vector of interleaved stereo chirp waveform with |frames| frames
// from |start_frequency_left| to |start_frequency_left| for left channel and
// from |start_frequency_right| to |end_frequency_right| for right channel,
// where |start_frequency_x| and |end_frequency_x| are normalized frequencies
// (2 * freq_in_hz / sample_rate) i.e. 0 - DC, 1 - nyquist.
// Equivalent to LinearChirp(frames,
//                          {start_frequency_left, start_frequency_right},
//                          {end_frequency_left, end_frequency_right})
AlignedBuffer<float> GetStereoChirp(int frames,
                                    float start_frequency_left,
                                    float end_frequency_left,
                                    float start_frequency_right,
                                    float end_frequency_right);

class PostProcessorTest : public ::testing::TestWithParam<int> {
 protected:
  PostProcessorTest();
  ~PostProcessorTest() override;

  int sample_rate_;
};

}  // namespace post_processor_test
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_POST_PROCESSOR_UNITTEST_H_
