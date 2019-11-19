// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "components/exo/wayland/clients/blur.h"
#include "components/exo/wayland/clients/simple.h"
#include "components/exo/wayland/clients/test/wayland_client_test.h"
#include "testing/perf/perf_test.h"

namespace {

using WaylandClientPerfTests = exo::WaylandClientTest;

// Test simple double-buffered client performance.
TEST_F(WaylandClientPerfTests, Simple) {
  const int kWarmUpFrames = 20;
  const int kTestFrames = 600;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  exo::wayland::clients::ClientBase::InitParams params;
  params.num_buffers = 2;  // Double-buffering.
  EXPECT_TRUE(params.FromCommandLine(*command_line));

  exo::wayland::clients::Simple client;
  EXPECT_TRUE(client.Init(params));

  client.Run(kWarmUpFrames, false, nullptr);

  exo::wayland::clients::Simple::PresentationFeedback feedback;
  auto start_time = base::Time::Now();
  client.Run(kTestFrames, false, &feedback);
  auto time_delta = base::Time::Now() - start_time;
  float fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "SimpleFrameRate", fps,
                         "frames/s", true);
  auto average_latency =
      feedback.num_frames_presented
          ? feedback.total_presentation_latency / feedback.num_frames_presented
          : base::TimeDelta::Max();
  perf_test::PrintResult(
      "WaylandClientPerfTests", "", "SimplePresentationLatency",
      static_cast<size_t>(average_latency.InMicroseconds()), "us", true);
}

class WaylandClientBlurPerfTests
    : public WaylandClientPerfTests,
      public ::testing::WithParamInterface<double> {
 public:
  WaylandClientBlurPerfTests() = default;
  ~WaylandClientBlurPerfTests() override = default;

  double max_sigma() const { return GetParam(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandClientBlurPerfTests);
};

INSTANTIATE_TEST_SUITE_P(,
                         WaylandClientBlurPerfTests,
                         testing::Values(4.0, 15.0));

TEST_P(WaylandClientBlurPerfTests, BlurSigma) {
  const int kWarmUpFrames = 20;
  const int kTestFrames = 600;
  const bool kOffscreen = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  exo::wayland::clients::ClientBase::InitParams params;
  EXPECT_TRUE(params.FromCommandLine(*command_line));

  exo::wayland::clients::Blur client;
  EXPECT_TRUE(client.Init(params));

  client.Run(0, 0, 0, false, kWarmUpFrames);

  auto start_time = base::Time::Now();
  client.Run(0, 0, max_sigma(), kOffscreen, kTestFrames);
  auto time_delta = base::Time::Now() - start_time;
  float fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma0", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(5, 5, max_sigma(), kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma5", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(15, 15, max_sigma(), kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma15", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(30, 30, max_sigma(), kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma30", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(50, 50, max_sigma(), kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigma50", fps,
                         "frames/s", true);
}

TEST_P(WaylandClientBlurPerfTests, BlurSigmaY) {
  const int kWarmUpFrames = 20;
  const int kTestFrames = 600;
  const bool kOffscreen = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  exo::wayland::clients::ClientBase::InitParams params;
  EXPECT_TRUE(params.FromCommandLine(*command_line));

  exo::wayland::clients::Blur client;
  EXPECT_TRUE(client.Init(params));

  client.Run(0, 0, 0, false, kWarmUpFrames);

  auto start_time = base::Time::Now();
  client.Run(0, 0, max_sigma(), kOffscreen, kTestFrames);
  auto time_delta = base::Time::Now() - start_time;
  float fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigmaY0", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(0, 5, max_sigma(), kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigmaY5", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(0, 10, max_sigma(), kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigmaY10", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(0, 25, max_sigma(), kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigmaY25", fps,
                         "frames/s", true);

  start_time = base::Time::Now();
  client.Run(0, 50, max_sigma(), kOffscreen, kTestFrames);
  time_delta = base::Time::Now() - start_time;
  fps = kTestFrames / time_delta.InSecondsF();
  perf_test::PrintResult("WaylandClientPerfTests", "", "BlurSigmaY50", fps,
                         "frames/s", true);
}

}  // namespace
