// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"

#include <optional>
#include <string>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/java_handler_thread.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/viz/common/features.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/gfx/android/fake_achoreographer_compat.h"

namespace viz {

class ExternalBeginFrameSourceAndroidTest : public ::testing::Test,
                                            public BeginFrameObserverBase {
 public:
  ~ExternalBeginFrameSourceAndroidTest() override {
    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExternalBeginFrameSourceAndroidTest::TeardownOnThread,
                       base::Unretained(this)));
    thread_->Stop();
  }

  void CreateThread() {
    thread_ = std::make_unique<base::android::JavaHandlerThread>("TestThread");
    thread_->Start();

    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExternalBeginFrameSourceAndroidTest::InitOnThread,
                       base::Unretained(this)));
  }

  void WaitForFrames(uint32_t frame_count) {
    frames_done_event_.Reset();
    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ExternalBeginFrameSourceAndroidTest::AddObserverOnThread,
            base::Unretained(this), frame_count));
    frames_done_event_.Wait();
  }

  ExternalBeginFrameSourceAndroid* begin_frame_source() {
    return begin_frame_source_.get();
  }

 private:
  void InitOnThread() {
    begin_frame_source_ = std::make_unique<ExternalBeginFrameSourceAndroid>(
        BeginFrameSource::kNotRestartableId, 60.f,
        /*requires_align_with_java=*/false);
  }

  void TeardownOnThread() { begin_frame_source_.reset(); }

  void AddObserverOnThread(uint32_t frame_count) {
    pending_frames_ = frame_count;
    begin_frame_source_->AddObserver(this);
  }

  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override {
    if (pending_frames_ == 0)
      return false;

    if (--pending_frames_ == 0) {
      begin_frame_source_->RemoveObserver(this);
      frames_done_event_.Signal();
    }
    return true;
  }
  void OnBeginFrameSourcePausedChanged(bool paused) override {}

  base::WaitableEvent frames_done_event_;
  std::unique_ptr<base::android::JavaHandlerThread> thread_;

  // Only accessed from TestThread.
  std::unique_ptr<ExternalBeginFrameSourceAndroid> begin_frame_source_;
  uint32_t pending_frames_ = 0;
};

TEST_F(ExternalBeginFrameSourceAndroidTest, DeliversFrames) {
  CreateThread();
  // Ensure we receive frames. When this returns we are no longer observing the
  // BeginFrameSource.
  WaitForFrames(10);
  // Ensure we can re-observe the same BeginFrameSource and get more frames.
  WaitForFrames(10);
}

TEST_F(ExternalBeginFrameSourceAndroidTest, DeliversFramesAfterIntervalChange) {
  CreateThread();
  // Ensure we receive frames. When this returns we are no longer observing the
  // BeginFrameSource.
  WaitForFrames(10);
  begin_frame_source()->UpdateRefreshRate(30.f);
  // Ensure we can re-observe the same BeginFrameSource and get more frames.
  WaitForFrames(10);
}

namespace {

using FrameCallbackData = gfx::FakeAChoreographerCompat::FrameCallbackData;
using FrameTimeline = gfx::FakeAChoreographerCompat::FrameTimeline;

struct FrameCallbackData64 {
  int64_t frame_time_nanos;
};

std::vector<FrameTimeline> CreateTimelines(
    const std::vector<int64_t>& expected_presentation_time_nanos) {
  std::vector<FrameTimeline> result;
  result.reserve(expected_presentation_time_nanos.size());
  for (size_t i = 0; i < expected_presentation_time_nanos.size(); ++i) {
    result.push_back(FrameTimeline{
        .vsync_id = static_cast<int64_t>(100 + i),
        .deadline_nanos = 9'000'000,
        .expected_presentation_time_nanos = expected_presentation_time_nanos[i],
    });
  }
  return result;
}

std::optional<base::TimeDelta> ExpectIfAtLeastBaklava(base::TimeDelta delta) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_BAKLAVA) {
    return std::nullopt;
  }
  return delta;
}

struct DeriveFeatureDisabled {};
struct DeriveFeatureEnabled {
  double snap_tolerance = 0.0;
};
using DeriveFeatureConfig =
    std::variant<DeriveFeatureDisabled, DeriveFeatureEnabled>;

struct AChoreographerImplTestParams {
  std::string test_name;
  DeriveFeatureConfig derive_feature_config;
  bool compat33_supported = true;
  int64_t os_provided_vsync_interval_nanos = 16'666'666;
  std::vector<float> display_supported_refresh_rates;
  std::variant<FrameCallbackData64, FrameCallbackData> callback_data;
  base::TimeDelta expected_interval;
  std::optional<base::TimeDelta> expected_deadline_derived_interval;
};

}  // namespace

class AChoreographerImplTest
    : public ::testing::TestWithParam<AChoreographerImplTestParams> {
 public:
  AChoreographerImplTest()
      : fake_compat_(/*compat_supported=*/true, GetParam().compat33_supported),
        begin_frame_source_(BeginFrameSource::kNotRestartableId,
                            /*refresh_rate=*/9999.f,
                            /*requires_align_with_java=*/false) {
    begin_frame_source_.AddObserver(&observer_);
  }

  ~AChoreographerImplTest() override {
    begin_frame_source_.RemoveObserver(&observer_);
  }

 protected:
  gfx::FakeAChoreographerCompat fake_compat_;
  ExternalBeginFrameSourceAndroid begin_frame_source_;
  MockBeginFrameObserver observer_;
};

TEST_P(AChoreographerImplTest, ExpectedInterval) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP() << "AChoreographerImpl requires at least Android R";
  }

  const AChoreographerImplTestParams& params = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;
  std::visit(absl::Overload{
                 [&](const DeriveFeatureDisabled&) {
                   scoped_feature_list.InitAndDisableFeature(
                       features::kCalculateDeadlineDerivedInterval);
                 },
                 [&](const DeriveFeatureEnabled& config) {
                   scoped_feature_list.InitAndEnableFeatureWithParameters(
                       features::kCalculateDeadlineDerivedInterval,
                       {{"snap_tolerance",
                         base::NumberToString(config.snap_tolerance)}});
                 },
             },
             params.derive_feature_config);

  if (!params.display_supported_refresh_rates.empty()) {
    base::flat_map<base::TimeDelta, float> supported_rates;
    for (float rate : params.display_supported_refresh_rates) {
      supported_rates[base::Hertz(rate)] = rate;
    }
    begin_frame_source_.SetSupportedRefreshRates(supported_rates);
  }

  fake_compat_.TriggerRefreshRateCallback(
      params.os_provided_vsync_interval_nanos);

  EXPECT_CALL(
      observer_,
      OnBeginFrame(testing::AllOf(
          testing::Field(&BeginFrameArgs::interval, params.expected_interval),
          testing::Field(&BeginFrameArgs::deadline_derived_interval,
                         params.expected_deadline_derived_interval))));

  std::visit(absl::Overload{
                 [&](const FrameCallbackData64& data) {
                   fake_compat_.TriggerFrameCallback64(data.frame_time_nanos);
                 },
                 [&](const FrameCallbackData& data) {
                   fake_compat_.TriggerVsync(data);
                 },
             },
             params.callback_data);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AChoreographerImplTest,
    ::testing::Values(
        AChoreographerImplTestParams{
            .test_name = "TimelinesNotSupported",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.0,
                },
            .compat33_supported = false,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .callback_data =
                FrameCallbackData64{
                    .frame_time_nanos = 10'000'000,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_deadline_derived_interval = std::nullopt,
        },
        AChoreographerImplTestParams{
            .test_name = "OnlyOneTimeline",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.0,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    .timelines = CreateTimelines({10'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_deadline_derived_interval = std::nullopt,
        },
        AChoreographerImplTestParams{
            .test_name = "FeatureDisabled",
            .derive_feature_config = DeriveFeatureDisabled{},
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    .timelines = CreateTimelines({10'000'000, 26'666'666}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_deadline_derived_interval = std::nullopt,
        },
        AChoreographerImplTestParams{
            .test_name = "TimelineDerivedTooShort",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.0,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 0.5 ms (< 1 ms threshold)
                    .timelines = CreateTimelines({10'000'000, 10'500'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval = std::nullopt,
        },
        AChoreographerImplTestParams{
            .test_name = "NoSupportedRates",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    .timelines = CreateTimelines({10'000'000, 22'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(12)),
        },
        AChoreographerImplTestParams{
            .test_name = "Snapped_WithinTolerance_CloserToHigher",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 11.5 ms ⇒ snaps to 11.111 ms (90 Hz), because
                    // |11.5 - 11.111| < |11.5 - 16.666|.
                    .timelines = CreateTimelines({10'000'000, 21'500'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(11.111)),
        },
        AChoreographerImplTestParams{
            .test_name = "Snapped_WithinTolerance_CloserToLower",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.25,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 13.9 ms ⇒ snaps to 16.666 ms (60 Hz), because
                    // |13.9 - 16.666| < |13.9 - 11.111|.
                    .timelines = CreateTimelines({10'000'000, 23'900'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(16.666)),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_BelowLowestSupported_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 20 ms ⇒ too far to snap to 16.666 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 30'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(20)),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_AboveHighestSupported_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 9 ms ⇒ too far to snap to 11.111 ms (90 Hz).
                    .timelines = CreateTimelines({10'000'000, 19'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(9)),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_BetweenSupported_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 14 ms ⇒ too far to snap to either 16.666 ms (60 Hz)
                    // or 11.111 ms (90 Hz).
                    .timelines = CreateTimelines({10'000'000, 24'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(14)),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_ZeroTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.0,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 16.8 ms, but still won't snap to 16.666 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 26'800'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(16.8)),
        }),
    [](const auto& info) { return info.param.test_name; });

}  // namespace viz
