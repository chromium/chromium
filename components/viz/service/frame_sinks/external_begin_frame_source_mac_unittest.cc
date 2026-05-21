// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mac.h"

#include <memory>
#include <utility>

#include "base/test/test_mock_time_task_runner.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "components/viz/test/fake_delay_based_time_source.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace viz {

class ExternalBeginFrameSourceMacTest : public testing::Test {
 public:
  ExternalBeginFrameSourceMacTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {
    output_surface_ = FakeSkiaOutputSurface::Create3d();
  }

  void SetUp() override {
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_);
    // Start with an invalid display ID to force fallback to timer.
    source_ = std::make_unique<ExternalBeginFrameSourceMac>(
        0, display::kInvalidDisplayId, output_surface_.get());

    // Replace the time source with a fake one that uses the mock clock.
    source_->time_source_ = std::make_unique<FakeDelayBasedTimeSource>(
        task_runner_->GetMockTickClock(), task_runner_.get());
    source_->time_source_->SetClient(source_.get());
    source_->time_source_->SetTimebaseAndInterval(task_runner_->NowTicks(),
                                                  base::Hertz(60));
  }

  std::unique_ptr<ui::VSyncCallbackMac> CreateVSyncCallback(
      ui::VSyncCallbackMac::Callback callback) {
    return std::unique_ptr<ui::VSyncCallbackMac>(new ui::VSyncCallbackMac(
        base::DoNothing(), std::move(callback), false));
  }

  scoped_refptr<ui::DisplayLinkMac>& display_link_mac() {
    return source_->display_link_mac_;
  }
  std::unique_ptr<ui::VSyncCallbackMac>& vsync_callback_mac() {
    return source_->vsync_callback_mac_;
  }
  std::unique_ptr<DelayBasedTimeSource>& time_source() {
    return source_->time_source_;
  }
  base::TimeDelta& preferred_interval() { return source_->preferred_interval_; }
  base::TimeDelta& min_refresh_interval() {
    return source_->min_refresh_interval_;
  }
  bool& needs_begin_frames() { return source_->needs_begin_frames_; }
  int& vsync_callback_keep_alive_counter() {
    return source_->vsync_callback_keep_alive_counter_;
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<ExternalBeginFrameSourceMac> source_;
};

// Test for Issue 514883342: Crash in ExternalBeginFrameSourceMac fallback
TEST_F(ExternalBeginFrameSourceMacTest, SetPreferredIntervalFallbackNoCrash) {
  // Ensure we are in fallback mode (timer created, display link null).
  source_->SetVSyncDisplayID(display::kInvalidDisplayId, /*force_update=*/true);
  EXPECT_FALSE(display_link_mac());
  EXPECT_TRUE(time_source());

  // This should not crash even if preferred_interval is set.
  source_->SetPreferredInterval(base::Hertz(30));
  EXPECT_EQ(preferred_interval(), base::Hertz(30));
}

// Test for Issue 484840438: Prevent zero frame interval crash
TEST_F(ExternalBeginFrameSourceMacTest, ZeroIntervalFallbackNoCrash) {
  source_->SetVSyncDisplayID(display::kInvalidDisplayId, /*force_update=*/true);

  // SetPreferredInterval with zero should use default interval and not crash.
  source_->SetPreferredInterval(base::TimeDelta());
  EXPECT_FALSE(preferred_interval().is_zero());
  EXPECT_EQ(preferred_interval(), BeginFrameArgs::DefaultInterval());
}

// Test for Issue 476456469: Fix a Mac crash - Hang in
// GetSupportedFrameIntervals
TEST_F(ExternalBeginFrameSourceMacTest, GetSupportedFrameIntervalsNoHang) {
  // Force min_refresh_interval_ to be zero to test hang prevention.
  min_refresh_interval() = base::TimeDelta();

  // This should not hang. It should return a default interval.
  auto intervals = source_->GetSupportedFrameIntervals(base::TimeDelta());
  EXPECT_FALSE(intervals.empty());
  EXPECT_TRUE(intervals.contains(BeginFrameArgs::DefaultInterval()));
}

// Test for kMaxKeepAliveCount logic
TEST_F(ExternalBeginFrameSourceMacTest, KeepAliveLogic) {
  // Let's manually set vsync_callback_mac_ to something.
  vsync_callback_mac() = CreateVSyncCallback(base::DoNothing());

  source_->OnNeedsBeginFrames(true);
  EXPECT_TRUE(needs_begin_frames());

  ui::VSyncParamsMac params;
  params.callback_times_valid = true;
  params.callback_timebase = base::TimeTicks::Now();
  params.callback_interval = base::Hertz(60);

  // Call once while needed.
  source_->OnDisplayLinkCallback(params);
  EXPECT_TRUE(vsync_callback_mac());
  EXPECT_EQ(vsync_callback_keep_alive_counter(), 0);

  // Stop needing begin frames.
  source_->OnNeedsBeginFrames(false);
  EXPECT_FALSE(needs_begin_frames());

  // Call 19 times, should still be alive.
  for (int i = 0; i < 19; ++i) {
    source_->OnDisplayLinkCallback(params);
    EXPECT_TRUE(vsync_callback_mac());
    EXPECT_EQ(vsync_callback_keep_alive_counter(), i + 1);
  }

  // 20th time should kill it.
  source_->OnDisplayLinkCallback(params);
  EXPECT_FALSE(vsync_callback_mac());
  EXPECT_EQ(vsync_callback_keep_alive_counter(), 20);
}

TEST_F(ExternalBeginFrameSourceMacTest, SendBeginFrame) {
  MockBeginFrameObserver obs;
  EXPECT_CALL(obs, OnBeginFrame(_)).Times(testing::AtLeast(1));

  source_->AddObserver(&obs);
  source_->OnNeedsBeginFrames(true);

  task_runner_->FastForwardBy(base::Milliseconds(100));

  source_->RemoveObserver(&obs);
}

}  // namespace viz
