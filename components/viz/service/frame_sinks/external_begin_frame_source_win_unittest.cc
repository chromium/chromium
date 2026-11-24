// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_win.h"

#include <vector>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr base::TimeDelta k240Hz = base::Hertz(240);
constexpr base::TimeDelta k144Hz = base::Hertz(144);
constexpr base::TimeDelta k100Hz = base::Hertz(100);
constexpr base::TimeDelta k60Hz = base::Hertz(60);
constexpr base::TimeDelta k30Hz = base::Hertz(30);

class RecordingBeginFrameObserver : public BeginFrameObserverBase {
 public:
  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override {
    received_args_.push_back(args);
    return true;
  }

  void OnBeginFrameSourcePausedChanged(bool paused) override {}

  const std::vector<BeginFrameArgs>& received_args() const {
    return received_args_;
  }
  void Clear() { received_args_.clear(); }

 private:
  std::vector<BeginFrameArgs> received_args_;
};

class TestExternalBeginFrameSourceWin : public ExternalBeginFrameSourceWin {
 public:
  TestExternalBeginFrameSourceWin(
      uint32_t restart_id,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : ExternalBeginFrameSourceWin(restart_id, std::move(task_runner)) {}

  void set_mock_monitor_interval(base::TimeDelta interval) {
    mock_monitor_interval_ = interval;
  }

  // Override to avoid registering with the real VSyncThreadWin during
  // deterministic unit tests.
  void OnNeedsBeginFrames(bool needs_begin_frames) override {}

 protected:
  base::TimeDelta QueryHardwareVSyncInterval() const override {
    return mock_monitor_interval_;
  }

 private:
  base::TimeDelta mock_monitor_interval_;
};

class ExternalBeginFrameSourceWinTest : public testing::Test {
 protected:
  void SetUp() override {
    source_ = std::make_unique<TestExternalBeginFrameSourceWin>(
        /*restart_id=*/1, base::SingleThreadTaskRunner::GetCurrentDefault());
    source_->AddObserver(&observer_);
    // Clear any initial MISSED BeginFrame from AddObserver().
    observer_.Clear();
  }

  void TearDown() override {
    source_->RemoveObserver(&observer_);
    source_.reset();
  }

  void SendVSync(base::TimeTicks vsync_time, base::TimeDelta vsync_interval) {
    source_->OnVSync(vsync_time, vsync_interval);
    base::RunLoop().RunUntilIdle();
  }

  void SetMonitorInterval(base::TimeDelta interval, int64_t display_id = 1) {
    source_->set_mock_monitor_interval(interval);
    source_->SetVSyncDisplayID(display_id, /*force_update=*/false);
  }

  std::unique_ptr<TestExternalBeginFrameSourceWin> source_;
  RecordingBeginFrameObserver observer_;
};

TEST_F(ExternalBeginFrameSourceWinTest, SlowerMonitorIntegerRatio) {
  // 240 Hz primary clock, 60 Hz secondary monitor (4:1 integer ratio).
  SetMonitorInterval(k60Hz);

  const base::TimeTicks start = base::TimeTicks::Now() + base::Seconds(1);
  // Send 12 ticks at 240 Hz (50 ms = 3 periods of 60 Hz).
  for (int i = 0; i < 12; ++i) {
    SendVSync(start + k240Hz * i, k240Hz);
  }

  ASSERT_EQ(observer_.received_args().size(), 3u);
  EXPECT_EQ(observer_.received_args()[0].frame_time, start);
  EXPECT_EQ(observer_.received_args()[1].frame_time, start + k240Hz * 4);
  EXPECT_EQ(observer_.received_args()[2].frame_time, start + k240Hz * 8);

  for (const auto& args : observer_.received_args()) {
    EXPECT_EQ(args.interval, k60Hz);
    EXPECT_EQ(args.unthrottled_interval, k60Hz);
  }
}

TEST_F(ExternalBeginFrameSourceWinTest, SlowerMonitorNonIntegerRatioNoDrift) {
  // 240 Hz primary clock, 100 Hz secondary monitor (2.4:1 non-integer ratio).
  SetMonitorInterval(k100Hz);

  const base::TimeTicks start = base::TimeTicks::Now() + base::Seconds(1);
  // 24 ticks at 240 Hz = 100 ms = exactly 10 frames at 100 Hz.
  for (int i = 0; i < 24; ++i) {
    SendVSync(start + k240Hz * i, k240Hz);
  }

  EXPECT_EQ(observer_.received_args().size(), 10u);
  for (const auto& args : observer_.received_args()) {
    EXPECT_EQ(args.interval, k100Hz);
    EXPECT_EQ(args.unthrottled_interval, k100Hz);
  }
}

TEST_F(ExternalBeginFrameSourceWinTest, FasterOrEqualMonitorNotThrottled) {
  // 60 Hz primary clock, 144 Hz secondary monitor: no throttling.
  SetMonitorInterval(k144Hz);

  const base::TimeTicks start = base::TimeTicks::Now() + base::Seconds(1);
  for (int i = 0; i < 5; ++i) {
    SendVSync(start + k60Hz * i, k60Hz);
  }

  ASSERT_EQ(observer_.received_args().size(), 5u);
  for (const auto& args : observer_.received_args()) {
    EXPECT_EQ(args.interval, k60Hz);
    EXPECT_EQ(args.unthrottled_interval, k60Hz);
  }
}

TEST_F(ExternalBeginFrameSourceWinTest,
       SetPreferredIntervalOnSlowerSecondaryMonitor) {
  // 240 Hz primary clock, 60 Hz secondary monitor.
  SetMonitorInterval(k60Hz);

  const base::TimeTicks start = base::TimeTicks::Now() + base::Seconds(1);

  // Requesting 60 Hz on a 60 Hz secondary monitor should NOT halve it to 30 Hz,
  // even though 60 Hz is > 2 * 240 Hz primary interval.
  source_->SetPreferredInterval(k60Hz);
  for (int i = 0; i < 12; ++i) {
    SendVSync(start + k240Hz * i, k240Hz);
  }
  ASSERT_EQ(observer_.received_args().size(), 3u);
  for (const auto& args : observer_.received_args()) {
    EXPECT_EQ(args.interval, k60Hz);
    EXPECT_EQ(args.unthrottled_interval, k60Hz);
  }

  // Requesting 30 Hz on a 60 Hz secondary monitor SHOULD halve it to 30 Hz
  // while still reporting 60 Hz as unthrottled_interval.
  observer_.Clear();
  source_->SetPreferredInterval(k30Hz);
  const base::TimeTicks phase2 = start + base::Seconds(1);
  // 24 ticks at 240 Hz = 100 ms = 3 frames at 30 Hz (ticks 0, 8, 16).
  for (int i = 0; i < 24; ++i) {
    SendVSync(phase2 + k240Hz * i, k240Hz);
  }
  ASSERT_EQ(observer_.received_args().size(), 3u);
  EXPECT_EQ(observer_.received_args()[0].frame_time, phase2);
  EXPECT_EQ(observer_.received_args()[1].frame_time, phase2 + k240Hz * 8);
  EXPECT_EQ(observer_.received_args()[2].frame_time, phase2 + k240Hz * 16);
  for (const auto& args : observer_.received_args()) {
    EXPECT_EQ(args.interval, k60Hz * 2);
    EXPECT_EQ(args.unthrottled_interval, k60Hz);
  }

  // Moving back to the 240 Hz monitor with 30 Hz preferred interval should
  // re-evaluate half-rate against 240 Hz (-> 120 Hz).
  observer_.Clear();
  SetMonitorInterval(k240Hz, /*display_id=*/2);
  const base::TimeTicks phase3 = phase2 + base::Seconds(1);
  for (int i = 0; i < 6; ++i) {
    SendVSync(phase3 + k240Hz * i, k240Hz);
  }
  ASSERT_EQ(observer_.received_args().size(), 3u);
  for (const auto& args : observer_.received_args()) {
    EXPECT_EQ(args.interval, k240Hz * 2);
    EXPECT_EQ(args.unthrottled_interval, k240Hz);
  }
}

TEST_F(ExternalBeginFrameSourceWinTest, StallRecoverySnapsWithoutBurst) {
  SetMonitorInterval(k60Hz);

  const base::TimeTicks start = base::TimeTicks::Now() + base::Seconds(1);
  SendVSync(start, k240Hz);
  ASSERT_EQ(observer_.received_args().size(), 1u);

  // Jump ahead by 500 ms (simulating a long stall), then send 4 normal ticks.
  const base::TimeTicks after_stall = start + base::Milliseconds(500);
  for (int i = 0; i < 4; ++i) {
    SendVSync(after_stall + k240Hz * i, k240Hz);
  }
  // Only 1 frame should be emitted across the 4 ticks (16.67 ms) after stall.
  EXPECT_EQ(observer_.received_args().size(), 2u);
}

}  // namespace
}  // namespace viz
