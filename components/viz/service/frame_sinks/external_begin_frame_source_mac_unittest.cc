// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mac.h"

#include <cmath>
#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "components/viz/test/fake_delay_based_time_source.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_features.h"

using testing::_;
using testing::Return;

namespace viz {

// Mocks the callback invoked when the VSync parameters (timebase and interval)
// are updated. Used to verify proper timing updates are sent to the client.
class MockUpdateVSyncParametersCallback {
 public:
  MOCK_METHOD(void, Run, (base::TimeTicks, base::TimeDelta));
  UpdateVSyncParametersCallback GetCallback() {
    return base::BindRepeating(&MockUpdateVSyncParametersCallback::Run,
                               base::Unretained(this));
  }
};

// Mock implementation of ui::DisplayLinkMac to simulate and verify macOS
// VSync ticks, refresh intervals, and dynamic refresh rate capabilities.
class MockDisplayLinkMac : public ui::DisplayLinkMac {
 public:
  // Sets the default mock behavior to simulate a 120Hz display with a minimum
  // refresh interval of 8.33333 ms.
  static constexpr base::TimeDelta kMinInterval = base::Hertz(120);

  MockDisplayLinkMac() {
    ON_CALL(*this, GetRefreshIntervalRange)
        .WillByDefault([](base::TimeDelta& min_interval,
                          base::TimeDelta& max_interval,
                          base::TimeDelta& granularity) {
          min_interval = kMinInterval;
          max_interval = base::Seconds(0.0416667);
          granularity = base::Seconds(0.00416667);
        });
    ON_CALL(*this, GetRefreshInterval)
        .WillByDefault(testing::Return(base::Hertz(120)));
    ON_CALL(*this, RegisterCallback)
        .WillByDefault(
            [](base::RepeatingCallback<void(ui::VSyncParamsMac)> callback) {
              return std::unique_ptr<ui::VSyncCallbackMac>(
                  new ui::VSyncCallbackMac(base::DoNothing(),
                                           std::move(callback), false));
            });
  }
  MOCK_METHOD(bool, NotifyEventAndCheckValidity, (), (override));
  MOCK_METHOD(std::unique_ptr<ui::VSyncCallbackMac>,
              RegisterCallback,
              (base::RepeatingCallback<void(ui::VSyncParamsMac)>),
              (override));
  MOCK_METHOD(base::TimeDelta, GetRefreshInterval, (), (const, override));
  MOCK_METHOD(void,
              GetRefreshIntervalRange,
              (base::TimeDelta&, base::TimeDelta&, base::TimeDelta&),
              (const, override));
  MOCK_METHOD(void, SetPreferredInterval, (base::TimeDelta), (override));
  MOCK_METHOD(base::TimeTicks, GetCurrentTime, (), (const, override));

 protected:
  ~MockDisplayLinkMac() override = default;

 private:
  friend class base::RefCountedThreadSafe<MockDisplayLinkMac>;
};

// A test wrapper subclass that overrides virtual methods of
// ExternalBeginFrameSourceMac to inject MockDisplayLinkMac instances and
// expose internals for white-box unit testing.
class ExternalBeginFrameSourceMacWrapper : public ExternalBeginFrameSourceMac {
 public:
  using ExternalBeginFrameSourceMac::ExternalBeginFrameSourceMac;

  scoped_refptr<ui::DisplayLinkMac> GetForDisplay(int64_t display_id) override {
    // For invalid display IDs, delegate to the production implementation to
    // trigger fallback code.
    if (display_id <= 0) {
      return ExternalBeginFrameSourceMac::GetForDisplay(display_id);
    }

    // Lazily instantiate and cache a mock DisplayLinkMac for valid display IDs.
    if (!display_link_mac_override_) {
      scoped_refptr<ui::DisplayLinkMac> mock_display_link =
          base::MakeRefCounted<testing::NiceMock<MockDisplayLinkMac>>();
      display_link_mac_override_ = std::move(mock_display_link);
    }

    return display_link_mac_override_;
  }

  base::TimeDelta GetMinimumFrameInterval() override {
    if (ui::DisplayLinkMac::SupportsDisplayLinkMacInBrowser()) {
      return MockDisplayLinkMac::kMinInterval;
    }
    return ExternalBeginFrameSourceMac::GetMinimumFrameInterval();
  }

  base::TimeDelta GetTimerDefaultFrameInterval() {
    if (ui::DisplayLinkMac::SupportsDisplayLinkMacInBrowser()) {
      return MockDisplayLinkMac::kMinInterval;
    }
    return BeginFrameArgs::DefaultInterval();
  }

  std::unique_ptr<DelayBasedTimeSource>& time_source() { return time_source_; }

  int& vsync_subsampling_factor() { return vsync_subsampling_factor_; }

  int64_t& display_id() { return display_id_; }

  scoped_refptr<ui::DisplayLinkMac>& display_link_mac() {
    return display_link_mac_;
  }

  ui::VSyncCallbackMac* vsync_callback_mac() {
    return vsync_callback_mac_.get();
  }
  const BeginFrameArgs& last_begin_frame_args() const {
    return last_begin_frame_args_;
  }
  base::TimeDelta& preferred_interval() { return preferred_interval_; }
  base::TimeDelta& min_refresh_interval() { return min_refresh_interval_; }

 private:
  scoped_refptr<ui::DisplayLinkMac> display_link_mac_override_;
};

class ExternalBeginFrameSourceMacTest : public testing::Test {
 public:
  ExternalBeginFrameSourceMacTest() {
    enable_feature.InitAndEnableFeature(
        display::features::kCADisplayLinkInBrowser);
    output_surface_ = FakeSkiaOutputSurface::Create3d();
  }

  void SetUp() override {
    // Start with an invalid display ID because override does not work in Ctor.
    source_ = std::make_unique<ExternalBeginFrameSourceMacWrapper>(
        0, display::kInvalidDisplayId, output_surface_.get());
    source_->SetUpdateVSyncParametersCallback(
        update_vsync_callback_.GetCallback());

    // Call SetVSyncDisplayID() with a valid id now to create a DisplayLinkMac.
    source_->SetVSyncDisplayID(/*display_id=*/1, /*force_update=*/false);
  }

  void TearDown() override { source_.reset(); }

  testing::NiceMock<MockUpdateVSyncParametersCallback> update_vsync_callback_;
  base::test::ScopedFeatureList enable_feature;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<ExternalBeginFrameSourceMacWrapper> source_;
};

// Tests successful configuration of valid vs. invalid VSync display IDs and
// the corresponding fallback behavior between DisplayLinkMac and the timer.
TEST_F(ExternalBeginFrameSourceMacTest, SetVSyncDisplayIDSuccess) {
  EXPECT_TRUE(source_->display_link_mac());
  EXPECT_FALSE(source_->time_source());

  // Now invalidate the display again. This should force fallback to timer.
  source_->SetVSyncDisplayID(display::kInvalidDisplayId,
                             /*force_update=*/false);
  EXPECT_FALSE(source_->display_link_mac());
  EXPECT_TRUE(source_->time_source());

  // Create a DisplayLink with a valid display ID.
  source_->SetVSyncDisplayID(/*display_id=*/1, /*force_update=*/false);
  EXPECT_TRUE(source_->display_link_mac());
  EXPECT_FALSE(source_->time_source());
}

// Tests that setting preferred intervals on a valid DisplayLink updates the
// cached preferred interval accordingly.
TEST_F(ExternalBeginFrameSourceMacTest, SetPreferredInterval) {
  EXPECT_TRUE(source_->display_link_mac());
  EXPECT_FALSE(source_->time_source());

  source_->SetPreferredInterval(base::Hertz(60));
  EXPECT_EQ(source_->preferred_interval(), base::Hertz(60));
  source_->SetPreferredInterval(base::Hertz(120));
  EXPECT_EQ(source_->preferred_interval(), base::Hertz(120));
}

// Verifies that preferred frame rates (e.g., 60Hz, 30Hz) correctly map to the
// corresponding VSync subsampling factors.
TEST_F(ExternalBeginFrameSourceMacTest, SetPreferredIntervalSubsampling) {
  source_->SetPreferredInterval(base::Hertz(60));
  EXPECT_EQ(source_->vsync_subsampling_factor(),
            std::round(base::Hertz(60) / MockDisplayLinkMac::kMinInterval));
  source_->SetPreferredInterval(base::Hertz(30));
  EXPECT_EQ(source_->vsync_subsampling_factor(),
            std::round(base::Hertz(30) / MockDisplayLinkMac::kMinInterval));
}

// Tests that setting a zero interval defaults back to the minimum refresh
// interval (e.g., 120Hz).
TEST_F(ExternalBeginFrameSourceMacTest, SetZeroInterval) {
  source_->SetPreferredInterval(base::TimeDelta());
  EXPECT_FALSE(source_->preferred_interval().is_zero());
  EXPECT_EQ(source_->preferred_interval(), source_->GetMinimumFrameInterval());
}

// Verifies that GetSupportedFrameIntervals returns the correct set of discrete
// intervals matching the subsampling logic.
TEST_F(ExternalBeginFrameSourceMacTest, GetSupportedFrameIntervals) {
  base::flat_set<base::TimeDelta> expected_intervals;
  if (source_->GetMinimumFrameInterval() == base::Hertz(120)) {
    expected_intervals = {base::Seconds(0.008333), base::Seconds(0.016666),
                          base::Seconds(0.033332), base::Seconds(0.066664)};
  } else {
    expected_intervals = {base::Seconds(0.016666), base::Seconds(0.033332),
                          base::Seconds(0.066664)};
  }

  base::flat_set<base::TimeDelta> intervals =
      source_->GetSupportedFrameIntervals(base::Hertz(120));
  EXPECT_FALSE(intervals.empty());
  EXPECT_TRUE(intervals == expected_intervals);
}

// Tests the keep-alive counter behavior where DisplayLink callbacks continue to
// be registered for a limited count after begin frames are no longer needed.
TEST_F(ExternalBeginFrameSourceMacTest, OnNeedsBeginFrames) {
  EXPECT_FALSE(source_->vsync_callback_mac());
  source_->OnNeedsBeginFrames(true);
  EXPECT_TRUE(source_->vsync_callback_mac());

  // Simulate multiple callbacks to trigger keep-alive unregistration.
  ui::VSyncParamsMac params;
  params.callback_times_valid = true;
  params.callback_timebase = base::TimeTicks::Now();
  params.callback_interval = base::Hertz(120);
  source_->OnDisplayLinkCallback(params);

  // Set to false. Should NOT unregister immediately due to keep-alive.
  source_->OnNeedsBeginFrames(false);
  // ExternalBeginFrameSourceMac::kMaxKeepAliveCount is 20.
  for (int i = 0; i < 19; ++i) {
    source_->OnDisplayLinkCallback(params);
    EXPECT_TRUE(source_->vsync_callback_mac());
  }
  source_->OnDisplayLinkCallback(params);
  EXPECT_FALSE(source_->vsync_callback_mac());
}

// Verifies that begin frame notifications are correctly dispatched to observers
// during active DisplayLink execution.
TEST_F(ExternalBeginFrameSourceMacTest, SendBeginFrame) {
  testing::NiceMock<MockBeginFrameObserver> obs;
  EXPECT_CALL(obs, OnBeginFrame(_)).Times(testing::AtLeast(1));
  source_->AddObserver(&obs);
  source_->OnNeedsBeginFrames(true);

  ui::VSyncParamsMac params;
  params.callback_times_valid = true;
  params.callback_timebase = base::TimeTicks::Now();
  params.callback_interval = base::Hertz(120);
  source_->OnDisplayLinkCallback(params);

  source_->RemoveObserver(&obs);
}

// Verifies transitioning from timer fallback back to a valid DisplayLink
// representation upon receiving CALayer updates.
TEST_F(ExternalBeginFrameSourceMacTest, UpdateVSyncDisplay) {
  // Start with DisplayLink failure and recreate a DisplayLinkMac after
  // UpdateVSyncDisplay.
  source_->SetVSyncDisplayID(display::kInvalidDisplayId,
                             /*force_update=*/false);
  source_->display_id() = 1;
  EXPECT_FALSE(source_->display_link_mac());
  EXPECT_TRUE(source_->time_source());

  source_->DidReceiveNewCALayerParams();
  source_->UpdateVSyncDisplay(/*display_id=*/1,
                              /*is_browser_vsync_supported=*/true);
  EXPECT_TRUE(source_->display_link_mac());
  EXPECT_FALSE(source_->time_source());
}

// Tests that incoming DisplayLink callbacks correctly update the last begin
// frame arguments' interval and timebase.
TEST_F(ExternalBeginFrameSourceMacTest, OnDisplayLinkCallback) {
  source_->OnNeedsBeginFrames(true);

  ui::VSyncParamsMac params;
  params.callback_times_valid = true;
  params.callback_timebase = base::TimeTicks::Now();
  params.callback_interval = base::Hertz(120);
  source_->OnDisplayLinkCallback(params);

  EXPECT_EQ(source_->last_begin_frame_args().interval,
            params.callback_interval);
  EXPECT_EQ(source_->last_begin_frame_args().frame_time,
            params.callback_timebase);
}

// Verifies that missed begin frame arguments are correctly calculated and
// returned with valid intervals on state transitions.
TEST_F(ExternalBeginFrameSourceMacTest, GetMissedBeginFrameArgs) {
  source_->OnNeedsBeginFrames(true);

  testing::NiceMock<MockBeginFrameObserver> obs;
  BeginFrameArgs args = source_->GetMissedBeginFrameArgs(&obs);
  EXPECT_TRUE(args.IsValid());
  EXPECT_EQ(args.interval, source_->GetMinimumFrameInterval());
  EXPECT_EQ(source_->last_begin_frame_args().interval,
            source_->GetMinimumFrameInterval());

  source_->OnNeedsBeginFrames(false);
  source_->OnNeedsBeginFrames(true);

  args = source_->GetMissedBeginFrameArgs(&obs);
  EXPECT_TRUE(args.IsValid());
  EXPECT_EQ(args.interval, source_->GetMinimumFrameInterval());
  EXPECT_EQ(source_->last_begin_frame_args().interval,
            source_->GetMinimumFrameInterval());
}

//----------------------------------------------------
// Test the fallback Timer when the display ID is invalid
//----------------------------------------------------

class ExternalBeginFrameSourceMacTimerTest
    : public ExternalBeginFrameSourceMacTest {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    // Use ScopedContext to ensure base::TimeTicks::Now() uses mock time.
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner_);

    // To prevent crashes with a null clock.
    task_runner_->AdvanceMockTickClock(base::Seconds(100));

    // Start with an invalid display ID to force fallback to timer.
    source_ = std::make_unique<ExternalBeginFrameSourceMacWrapper>(
        0, display::kInvalidDisplayId, output_surface_.get());

    // Simulate a failing ExternalBeginFrameSourceMac case with a valid
    // display ID, 100. We cannot use `SetVSyncDisplayID(kInvalidDisplayId
    // /*force_update=*/true)` because GetMinimumFrameInterval() returns the
    // refresh rate from the display id and we don't have a valid id here.
    source_->display_id() = 100;
    base::TimeDelta min_interval = source_->GetMinimumFrameInterval();
    source_->preferred_interval() = source_->min_refresh_interval() =
        min_interval;

    // Replace the timer with FakeDelayBasedTimeSource.
    source_->time_source() = std::make_unique<FakeDelayBasedTimeSource>(
        task_runner_->GetMockTickClock(), task_runner_.get());
    source_->time_source()->SetClient(source_.get());
    source_->time_source()->SetTimebaseAndInterval(task_runner_->NowTicks(),
                                                   min_interval);

    source_->SetUpdateVSyncParametersCallback(
        update_vsync_callback_.GetCallback());
  }

  void TearDown() override {
    source_.reset();
    task_runner_ = nullptr;
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

// Tests that an invalid display ID correctly falls back to using the timer when
// DisplayLink fails or is unavailable.
TEST_F(ExternalBeginFrameSourceMacTimerTest, SetVSyncDisplayIDFailure) {
  EXPECT_FALSE(source_->display_link_mac());
  EXPECT_TRUE(source_->time_source());

  // Create a DisplayLink with a valid display ID.
  source_->SetVSyncDisplayID(/*display_id=*/1, /*force_update=*/false);
  EXPECT_TRUE(source_->display_link_mac());
  EXPECT_FALSE(source_->time_source());

  // Now invalidate the display again. This should force fallback to timer.
  source_->SetVSyncDisplayID(display::kInvalidDisplayId,
                             /*force_update=*/false);
  EXPECT_FALSE(source_->display_link_mac());
  EXPECT_TRUE(source_->time_source());
}

// Verifies preferred interval settings while in timer fallback mode.
TEST_F(ExternalBeginFrameSourceMacTimerTest, SetPreferredInterval) {
  source_->SetPreferredInterval(base::Hertz(30));
  EXPECT_EQ(source_->preferred_interval(), base::Hertz(30));
  source_->SetPreferredInterval(base::Hertz(60));
  EXPECT_EQ(source_->preferred_interval(), base::Hertz(60));
}

// Verifies that a zero preferred interval correctly resets to the timer's
// default frame interval.
TEST_F(ExternalBeginFrameSourceMacTimerTest, SetZeroInterval) {
  source_->SetPreferredInterval(base::TimeDelta());
  EXPECT_FALSE(source_->preferred_interval().is_zero());
  EXPECT_EQ(source_->preferred_interval(),
            source_->GetTimerDefaultFrameInterval());
}

// Verifies that supported frame intervals in timer mode map to valid discrete
// increments of the default fallback rate.
TEST_F(ExternalBeginFrameSourceMacTimerTest, GetSupportedFrameIntervals) {
  base::flat_set<base::TimeDelta> expected_intervals;
  if (source_->GetMinimumFrameInterval() == base::Hertz(120)) {
    expected_intervals = {base::Seconds(0.008333), base::Seconds(0.016666),
                          base::Seconds(0.033332), base::Seconds(0.066664)};
  } else {
    expected_intervals = {base::Seconds(0.016666), base::Seconds(0.033332),
                          base::Seconds(0.066664)};
  }

  base::flat_set<base::TimeDelta> intervals =
      source_->GetSupportedFrameIntervals(base::Hertz(120));
  EXPECT_FALSE(intervals.empty());
  EXPECT_TRUE(intervals == expected_intervals);
}

// Verifies that observers receive begin frame callbacks during timer fallback
// execution.
TEST_F(ExternalBeginFrameSourceMacTimerTest, SendBeginFrame) {
  testing::NiceMock<MockBeginFrameObserver> obs;
  EXPECT_CALL(obs, OnBeginFrame(_)).Times(testing::AtLeast(1));
  source_->AddObserver(&obs);
  source_->OnNeedsBeginFrames(true);
  task_runner_->FastForwardBy(base::Milliseconds(100));
  source_->RemoveObserver(&obs);
}

// Verifies the frame arguments (timebase/interval) constructed during a timer
// tick.
TEST_F(ExternalBeginFrameSourceMacTimerTest, OnTimerTickArguments) {
  source_->OnNeedsBeginFrames(true);
  base::TimeDelta time_elapsed = base::Hertz(60) * 1.5;
  task_runner_->FastForwardBy(time_elapsed);
  EXPECT_TRUE(source_->last_begin_frame_args().IsValid());
  EXPECT_EQ(source_->last_begin_frame_args().interval,
            source_->GetTimerDefaultFrameInterval());
}

// Verifies calculation of missed begin frame arguments when starting
// NeedsBeginFrames.
TEST_F(ExternalBeginFrameSourceMacTimerTest, GetMissedBeginFrameArgs) {
  source_->OnNeedsBeginFrames(true);
  testing::NiceMock<MockBeginFrameObserver> obs;
  BeginFrameArgs args = source_->GetMissedBeginFrameArgs(&obs);
  EXPECT_EQ(args.interval, source_->GetTimerDefaultFrameInterval());
  EXPECT_EQ(source_->last_begin_frame_args().interval,
            source_->GetTimerDefaultFrameInterval());

  task_runner_->FastForwardBy(base::Hertz(60) * 2);
  source_->OnNeedsBeginFrames(false);
  task_runner_->FastForwardBy(base::Seconds(10));

  source_->OnNeedsBeginFrames(true);
  args = source_->GetMissedBeginFrameArgs(&obs);
  EXPECT_EQ(args.interval, source_->GetTimerDefaultFrameInterval());
  EXPECT_TRUE(source_->last_begin_frame_args().IsValid());
  EXPECT_EQ(source_->last_begin_frame_args().interval,
            source_->GetTimerDefaultFrameInterval());
}

// Tests starting and stopping the underlying timer source when needing or no
// longer needing begin frames.
TEST_F(ExternalBeginFrameSourceMacTimerTest, OnNeedsBeginFrames) {
  source_->OnNeedsBeginFrames(true);
  EXPECT_TRUE(source_->time_source()->Active());

  source_->OnNeedsBeginFrames(false);
  EXPECT_FALSE(source_->time_source()->Active());
}

}  // namespace viz
