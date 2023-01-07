// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "components/exo/wayland/clients/blur.h"
#include "components/exo/wayland/clients/simple.h"
#include "components/exo/wayland/clients/test/wayland_client_test.h"
#include "testing/perf/perf_result_reporter.h"

namespace {

using WaylandClientPerfTests = exo::WaylandClientTest;

constexpr char kMetricPrefixWaylandClient[] = "WaylandClient.";
constexpr char kMetricFramerate[] = "framerate";
constexpr char kMetricPresentationLatency[] = "presentation_latency";
constexpr char kStorySimple[] = "simple";
constexpr char kStoryPrefixBlurSigma[] = "blur_sigma_%d";
constexpr char kStoryPrefixBlurSigmaY[] = "blur_sigma_y_%d";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixWaylandClient, story);
  reporter.RegisterImportantMetric(kMetricFramerate, "fps");
  reporter.RegisterImportantMetric(kMetricPresentationLatency, "us");
  return reporter;
}

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
  auto reporter = SetUpReporter(kStorySimple);
  reporter.AddResult(kMetricFramerate, fps);

  auto average_latency =
      feedback.num_frames_presented
          ? feedback.total_presentation_latency / feedback.num_frames_presented
          : base::TimeDelta::Max();
  reporter.AddResult(kMetricPresentationLatency, average_latency);
}

class WaylandClientBlurPerfTests
    : public WaylandClientPerfTests,
      public ::testing::WithParamInterface<double> {
 public:
  WaylandClientBlurPerfTests() = default;

  WaylandClientBlurPerfTests(const WaylandClientBlurPerfTests&) = delete;
  WaylandClientBlurPerfTests& operator=(const WaylandClientBlurPerfTests&) =
      delete;

  ~WaylandClientBlurPerfTests() override = default;

  double max_sigma() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
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

  constexpr int blur_values[] = {0, 5, 15, 30, 50};

  for (int bv : blur_values) {
    auto start_time = base::Time::Now();
    client.Run(bv, bv, max_sigma(), kOffscreen, kTestFrames);
    auto time_delta = base::Time::Now() - start_time;
    float fps = kTestFrames / time_delta.InSecondsF();
    SetUpReporter(base::StringPrintf(kStoryPrefixBlurSigma, bv))
        .AddResult(kMetricFramerate, fps);
  }
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

  constexpr int blur_values[] = {0, 5, 10, 25, 50};

  for (int bv : blur_values) {
    auto start_time = base::Time::Now();
    client.Run(0, bv, max_sigma(), kOffscreen, kTestFrames);
    auto time_delta = base::Time::Now() - start_time;
    float fps = kTestFrames / time_delta.InSecondsF();
    SetUpReporter(base::StringPrintf(kStoryPrefixBlurSigmaY, bv))
        .AddResult(kMetricFramerate, fps);
  }
}

}  // namespace
