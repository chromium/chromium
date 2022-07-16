// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/fuchsia/mixer_output_stream_fuchsia.h"

#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

constexpr int kSampleRate = 48000;
constexpr int kNumChannels = 2;

class MixerOutputStreamFuchsiaTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  MixerOutputStreamFuchsia output_;
};

TEST_F(MixerOutputStreamFuchsiaTest, StartAndStop) {
  EXPECT_TRUE(output_.Start(kSampleRate, kNumChannels));
  EXPECT_EQ(output_.GetSampleRate(), kSampleRate);
  output_.Stop();
}

TEST_F(MixerOutputStreamFuchsiaTest, Play1s) {
  EXPECT_TRUE(output_.Start(kSampleRate, kNumChannels));

  constexpr auto kTestStreamDuration = base::Milliseconds(300);
  constexpr float kSignalFrequencyHz = 1000;

  auto started = base::TimeTicks::Now();

  int samples_to_play =
      base::ClampFloor(kSampleRate * kTestStreamDuration.InSecondsF());
  int pos = 0;
  while (pos < samples_to_play) {
    std::vector<float> buffer;
    int num_frames = output_.OptimalWriteFramesCount();
    buffer.resize(num_frames * kNumChannels);
    for (int i = 0; i < num_frames; ++i) {
      float v = sin(2 * M_PI * pos * kSignalFrequencyHz / kSampleRate);
      for (int c = 0; c < kNumChannels; ++c) {
        buffer[i * kNumChannels + c] = v;
      }
      pos += 1;
    }
    bool interrupted = true;
    EXPECT_TRUE(output_.Write(buffer.data(), buffer.size(), &interrupted));

    // Run message loop to process async events.
    base::RunLoop().RunUntilIdle();
  }

  auto ended = base::TimeTicks::Now();

  // Verify that Write() was blocking, allowing 100ms for buffering.
  EXPECT_GT(ended - started, kTestStreamDuration - base::Milliseconds(100));

  output_.Stop();
}

TEST_F(MixerOutputStreamFuchsiaTest, PlaybackInterrupted) {
  EXPECT_TRUE(output_.Start(kSampleRate, kNumChannels));

  std::vector<float> buffer;
  int num_frames = output_.OptimalWriteFramesCount();
  buffer.resize(num_frames * kNumChannels);

  bool interrupted = true;

  // First Write() always returns interrupted = false.
  EXPECT_TRUE(output_.Write(buffer.data(), buffer.size(), &interrupted));
  EXPECT_FALSE(interrupted);

  interrupted = true;
  // Repeated Write() is expected to return interrupted = false.
  EXPECT_TRUE(output_.Write(buffer.data(), buffer.size(), &interrupted));
  EXPECT_FALSE(interrupted);

  // Run message loop for 100ms before calling Write() again.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
  run_loop.Run();

  // Write() is called to late, expect interrupted = true.
  interrupted = false;
  EXPECT_TRUE(output_.Write(buffer.data(), buffer.size(), &interrupted));
  EXPECT_TRUE(interrupted);

  output_.Stop();
}

}  // namespace media
}  // namespace chromecast
