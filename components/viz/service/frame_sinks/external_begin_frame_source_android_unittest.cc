// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/java_handler_thread.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/tracing/test_trace_processor.h"
#include "base/time/time.h"
#include "components/viz/common/features.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "testing/gmock/include/gmock/gmock.h"
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

using DeadlineDerivedIntervalResult =
    ExternalBeginFrameSourceAndroid::DeadlineDerivedIntervalResult;
using enum DeadlineDerivedIntervalResult;
using FrameCallbackData = gfx::FakeAChoreographerCompat::FrameCallbackData;
using FrameTimeline = gfx::FakeAChoreographerCompat::FrameTimeline;
using QueryResult = base::test::TestTraceProcessor::QueryResult;

constexpr const char kHistogramName[] =
    "Android.AChoreographer.DeadlineDerivedIntervalResult";

constexpr const char kTraceEventArgsQuery[] = R"(
  SELECT key, display_value
  FROM slices
  JOIN args USING (arg_set_id)
  WHERE slices.name = 'AChoreographerImpl::CalculateDeadlineDerivedInterval'
    AND key LIKE 'android_deadline_derived_interval_calculation.%'
  ORDER BY key ASC
)";

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

bool IsAtLeastBaklava() {
  return base::android::android_info::sdk_int() >=
         base::android::android_info::SDK_VERSION_BAKLAVA;
}

std::optional<base::TimeDelta> ExpectIfAtLeastBaklava(base::TimeDelta delta) {
  if (!IsAtLeastBaklava()) {
    return std::nullopt;
  }
  return delta;
}

DeadlineDerivedIntervalResult ExpectResultIfAtLeastBaklava(
    DeadlineDerivedIntervalResult result) {
  if (!IsAtLeastBaklava()) {
    return kNotCalculatedUnsupportedAndroidVersion;
  }
  return result;
}

struct ExpectedTrace {
  std::string result;
  std::optional<int64_t> present_deltas_difference_us;
  std::optional<int64_t> closest_supported_interval_us;
  std::optional<int64_t> deadline_derived_interval_us;
};

ExpectedTrace ExpectTraceIfAtLeastBaklava(ExpectedTrace trace) {
  if (!IsAtLeastBaklava()) {
    return ExpectedTrace{
        .result = "NOT_CALCULATED_UNSUPPORTED_ANDROID_VERSION",
    };
  }
  return trace;
}

struct DeriveFeatureDisabled {};
struct DeriveFeatureEnabled {
  double snap_tolerance = 0.0;
  bool snap_to_os_provided = false;
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
  DeadlineDerivedIntervalResult expected_result;
  bool expected_trial_active;
  ExpectedTrace expected_trace;
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
  base::test::TracingEnvironment tracing_environment_;
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

  base::test::ScopedFeatureList isolated_trial_scope;
  base::test::ScopedFeatureList scoped_feature_list;
  std::visit(
      absl::Overload{
          [&](const DeriveFeatureDisabled&) {
            // Unlike `InitAndEnableFeatureWithParameters()`,
            // `ScopedFeatureList::InitAndDisableFeature()` doesn't associate a
            // `base::FieldTrial` with the disabled feature, which would make
            // `FeatureList::GetFieldTrial()` return `nullptr`. Manually
            // register a disabled field-trial override (mirroring a Control
            // arm) so `IsTrialActive()` can be checked unconditionally below.
            // `isolated_trial_scope.Init()` backs up the global
            // `FieldTrialList` (which `InitWithFeatureList()` alone doesn't do)
            // so this trial doesn't leak across parameterized test runs.
            isolated_trial_scope.Init();
            base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
                "CalculateDeadlineDerivedIntervalDisabledTrial", "Control");
            auto feature_list = std::make_unique<base::FeatureList>();
            feature_list->RegisterFieldTrialOverride(
                features::kCalculateDeadlineDerivedInterval.name,
                base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
            scoped_feature_list.InitWithFeatureList(std::move(feature_list));
          },
          [&](const DeriveFeatureEnabled& config) {
            scoped_feature_list.InitAndEnableFeatureWithParameters(
                features::kCalculateDeadlineDerivedInterval,
                {{"snap_tolerance",
                  base::NumberToString(config.snap_tolerance)},
                 {"snap_to_os_provided",
                  config.snap_to_os_provided ? "true" : "false"}});
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

  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;
  base::HistogramTester histogram_tester;
  base::test::TestTraceProcessor trace_processor;
  trace_processor.StartTrace("input.scrolling");

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

  histogram_tester.ExpectUniqueSample(kHistogramName, params.expected_result,
                                      1);

  base::FieldTrial* trial = base::FeatureList::GetFieldTrial(
      features::kCalculateDeadlineDerivedInterval);
  ASSERT_NE(trial, nullptr);
  EXPECT_EQ(base::FieldTrialList::IsTrialActive(trial->trial_name()),
            params.expected_trial_active);

  absl::Status status = trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto trace_result = trace_processor.RunQuery(kTraceEventArgsQuery);
  ASSERT_TRUE(trace_result.has_value()) << trace_result.error();

  QueryResult expected_rows = {{"key", "display_value"}};
  auto with_prefix = [](const char* arg_name) {
    constexpr std::string_view kPrefix =
        "android_deadline_derived_interval_calculation.";
    return std::string(kPrefix) + arg_name;
  };

  if (params.expected_trace.closest_supported_interval_us.has_value()) {
    expected_rows.push_back(
        {with_prefix("closest_supported_interval_us"),
         base::NumberToString(
             *params.expected_trace.closest_supported_interval_us)});
  }
  if (params.expected_trace.deadline_derived_interval_us.has_value()) {
    expected_rows.push_back(
        {with_prefix("deadline_derived_interval_us"),
         base::NumberToString(
             *params.expected_trace.deadline_derived_interval_us)});
  }
  expected_rows.push_back(
      {with_prefix("os_provided_interval_us"),
       base::NumberToString(params.expected_interval.InMicroseconds())});
  if (params.expected_trace.present_deltas_difference_us.has_value()) {
    expected_rows.push_back(
        {with_prefix("present_deltas_difference_us"),
         base::NumberToString(
             *params.expected_trace.present_deltas_difference_us)});
  }
  expected_rows.push_back(
      {with_prefix("result"), params.expected_trace.result});

  EXPECT_EQ(*trace_result, expected_rows);
}

TEST(AChoreographerImplSubsamplingTest,
     HistogramsNotRecordedWhenSubsampledOut) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP() << "AChoreographerImpl requires at least Android R";
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kCalculateDeadlineDerivedInterval);
  base::test::TracingEnvironment tracing_environment;

  gfx::FakeAChoreographerCompat fake_compat(/*compat_supported=*/true,
                                            /*compat33_supported=*/true);
  ExternalBeginFrameSourceAndroid begin_frame_source(
      BeginFrameSource::kNotRestartableId,
      /*refresh_rate=*/9999.f,
      /*requires_align_with_java=*/false);
  MockBeginFrameObserver observer;
  begin_frame_source.AddObserver(&observer);

  base::HistogramTester histogram_tester;
  base::test::TestTraceProcessor trace_processor;
  trace_processor.StartTrace("input.scrolling");

  {
    base::MetricsSubSampler::ScopedNeverSampleForTesting never_sample;
    EXPECT_CALL(observer, OnBeginFrame(testing::_));
    fake_compat.TriggerVsync({
        .frame_time_nanos = 10'000'000,
        .timelines = CreateTimelines({10'000'000, 26'666'666}),
        .preferred_index = 0,
    });
  }

  // Disabling VSync notifications while a callback is already pending must
  // neither notify observers nor emit UMA/trace events when that callback
  // fires.
  begin_frame_source.RemoveObserver(&observer);
  {
    base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;
    fake_compat.TriggerVsync({
        .frame_time_nanos = 26'666'666,
        .timelines = CreateTimelines({26'666'666, 43'333'332}),
        .preferred_index = 0,
    });
  }

  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  absl::Status status = trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
  auto trace_result = trace_processor.RunQuery(R"(
    SELECT COUNT(*) FROM slices
    WHERE name = 'AChoreographerImpl::CalculateDeadlineDerivedInterval'
  )");
  ASSERT_TRUE(trace_result.has_value()) << trace_result.error();
  EXPECT_EQ(*trace_result, QueryResult({{"COUNT(*)"}, {"1"}}));
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
            .expected_result = ExpectResultIfAtLeastBaklava(
                kNotCalculatedTimelinesNotSupported),
            .expected_trial_active = false,
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "NOT_CALCULATED_TIMELINES_NOT_SUPPORTED",
            }),
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
            .expected_result =
                ExpectResultIfAtLeastBaklava(kNotCalculatedOnlyOneTimeline),
            .expected_trial_active = false,
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "NOT_CALCULATED_ONLY_ONE_TIMELINE",
            }),
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
            .expected_result =
                ExpectResultIfAtLeastBaklava(kNotCalculatedFeatureDisabled),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "NOT_CALCULATED_FEATURE_DISABLED",
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "TimelineDerivedTooShort",
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
                    // Diff 0.5 ms (< 1 ms threshold)
                    .timelines = CreateTimelines({10'000'000, 10'500'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval = std::nullopt,
            .expected_result =
                ExpectResultIfAtLeastBaklava(kNotCalculatedIntervalTooShort),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "NOT_CALCULATED_INTERVAL_TOO_SHORT",
                .present_deltas_difference_us = 500,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "NoSupportedRates",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = false,
                },
            .compat33_supported = true,
            // Within 10% tolerance of 12 ms, but ignored because
            // `snap_to_os_provided` is false.
            .os_provided_vsync_interval_nanos = 12'500'000,
            .display_supported_refresh_rates = {},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    .timelines = CreateTimelines({10'000'000, 22'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(12.5),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(12)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedUnsnappedNoSnapCandidates),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_UNSNAPPED_NO_SNAP_CANDIDATES",
                .present_deltas_difference_us = 12'000,
                .deadline_derived_interval_us = 12'000,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Snapped_WithinTolerance_CloserToHigher",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = false,
                },
            .compat33_supported = true,
            // Closer to 11.5 ms than 11.111 ms, but ignored because
            // `snap_to_os_provided` is false.
            .os_provided_vsync_interval_nanos = 11'400'000,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 11.5 ms ⇒ snaps to 11.111 ms (90 Hz), because
                    // |11.5 - 11.111| < |11.5 - 16.666|.
                    .timelines = CreateTimelines({10'000'000, 21'500'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(11.4),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(11.111)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToSupportedOsProvidedDisabledOrOutsideSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result =
                    "CALCULATED_SNAPPED_TO_SUPPORTED_OS_PROVIDED_DISABLED_OR_"
                    "OUTSIDE_SNAP_TOLERANCE",
                .present_deltas_difference_us = 11'500,
                .closest_supported_interval_us = 11'111,
                .deadline_derived_interval_us = 11'111,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Snapped_WithinTolerance_CloserToLower",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.25,
                    .snap_to_os_provided = false,
                },
            .compat33_supported = true,
            // Closer to 13.9 ms than 16.666 ms, but ignored because
            // `snap_to_os_provided` is false.
            .os_provided_vsync_interval_nanos = 14'000'000,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 13.9 ms ⇒ snaps to 16.666 ms (60 Hz), because
                    // |13.9 - 16.666| < |13.9 - 11.111|.
                    .timelines = CreateTimelines({10'000'000, 23'900'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(14),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(16.666)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToSupportedOsProvidedDisabledOrOutsideSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result =
                    "CALCULATED_SNAPPED_TO_SUPPORTED_OS_PROVIDED_DISABLED_OR_"
                    "OUTSIDE_SNAP_TOLERANCE",
                .present_deltas_difference_us = 13'900,
                .closest_supported_interval_us = 16'666,
                .deadline_derived_interval_us = 16'666,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Snapped_ExactSnapToleranceBoundary",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.25,
                    .snap_to_os_provided = false,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {100.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 8 ms vs supported 10 ms (100 Hz) ⇒ distance 2 ms ==
                    // `0.25 * 8 ms` (`max_snap_distance`), snaps to 10 ms.
                    .timelines = CreateTimelines({10'000'000, 18'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(10)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToSupportedOsProvidedDisabledOrOutsideSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result =
                    "CALCULATED_SNAPPED_TO_SUPPORTED_OS_PROVIDED_DISABLED_OR_"
                    "OUTSIDE_SNAP_TOLERANCE",
                .present_deltas_difference_us = 8'000,
                .closest_supported_interval_us = 10'000,
                .deadline_derived_interval_us = 10'000,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_BelowLowestSupported_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = false,
                },
            .compat33_supported = true,
            // Within 10% tolerance of 20 ms, but ignored because
            // `snap_to_os_provided` is false.
            .os_provided_vsync_interval_nanos = 20'833'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 20 ms ⇒ too far to snap to 16.666 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 30'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(20.833),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(20)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedUnsnappedOutsideSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_UNSNAPPED_OUTSIDE_SNAP_TOLERANCE",
                .present_deltas_difference_us = 20'000,
                .closest_supported_interval_us = 16'666,
                .deadline_derived_interval_us = 20'000,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_AboveHighestSupported_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = false,
                },
            .compat33_supported = true,
            // Within 10% tolerance of 9 ms, but ignored because
            // `snap_to_os_provided` is false.
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 9 ms ⇒ too far to snap to 11.111 ms (90 Hz).
                    .timelines = CreateTimelines({10'000'000, 19'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(9)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedUnsnappedOutsideSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_UNSNAPPED_OUTSIDE_SNAP_TOLERANCE",
                .present_deltas_difference_us = 9'000,
                .closest_supported_interval_us = 11'111,
                .deadline_derived_interval_us = 9'000,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_BetweenSupported_OutsideSnapTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = false,
                },
            .compat33_supported = true,
            // Within 10% tolerance of 14 ms, but ignored because
            // `snap_to_os_provided` is false.
            .os_provided_vsync_interval_nanos = 13'888'888,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 14 ms ⇒ too far to snap to either 16.666 ms (60 Hz)
                    // or 11.111 ms (90 Hz).
                    .timelines = CreateTimelines({10'000'000, 24'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(13.888),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(14)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedUnsnappedOutsideSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_UNSNAPPED_OUTSIDE_SNAP_TOLERANCE",
                .present_deltas_difference_us = 14'000,
                .closest_supported_interval_us = 16'666,
                .deadline_derived_interval_us = 14'000,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_ZeroTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.0,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 16.8 ms, won't search supported intervals or compare
                    // with OS-provided interval when `snap_tolerance == 0.0`.
                    .timelines = CreateTimelines({10'000'000, 26'800'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(16.8)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedUnsnappedZeroSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_UNSNAPPED_ZERO_SNAP_TOLERANCE",
                .present_deltas_difference_us = 16'800,
                .deadline_derived_interval_us = 16'800,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_ZeroTolerance_ExactMatch",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.0,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'000,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 16.666 ms (exact match with both 60 Hz and
                    // OS-provided), still immediately returns unsnapped without
                    // searching supported intervals when `snap_tolerance == 0`.
                    .timelines = CreateTimelines({10'000'000, 26'666'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(16.666)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedUnsnappedZeroSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_UNSNAPPED_ZERO_SNAP_TOLERANCE",
                .present_deltas_difference_us = 16'666,
                .deadline_derived_interval_us = 16'666,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_NegativeTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = -0.1,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'000,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Negative `snap_tolerance` also short-circuits without
                    // searching supported intervals or comparing OS-provided.
                    .timelines = CreateTimelines({10'000'000, 26'666'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(16.666)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedUnsnappedZeroSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_UNSNAPPED_ZERO_SNAP_TOLERANCE",
                .present_deltas_difference_us = 16'666,
                .deadline_derived_interval_us = 16'666,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "SnappedToOsProvided_NoSupportedRates",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 12'500'000,
            .display_supported_refresh_rates = {},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 12 ms ⇒ snaps to OS-provided 12.5 ms (80 Hz).
                    .timelines = CreateTimelines({10'000'000, 22'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(12.5),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(12.5)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToOsProvidedNoSupportedIntervals),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_SNAPPED_TO_OS_PROVIDED_NO_SUPPORTED_"
                          "INTERVALS",
                .present_deltas_difference_us = 12'000,
                .deadline_derived_interval_us = 12'500,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "SnappedToOsProvided_SupportedOutsideTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 12'500'000,
            .display_supported_refresh_rates = {60.f, 120.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 12 ms ⇒ too far from 8.333 ms and 16.666 ms, snaps
                    // to OS-provided 12.5 ms (80 Hz).
                    .timelines = CreateTimelines({10'000'000, 22'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(12.5),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(12.5)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToOsProvidedSupportedOutsideSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_SNAPPED_TO_OS_PROVIDED_SUPPORTED_"
                          "OUTSIDE_SNAP_TOLERANCE",
                .present_deltas_difference_us = 12'000,
                .closest_supported_interval_us = 8'333,
                .deadline_derived_interval_us = 12'500,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "SnappedToSupported_OsProvidedOutsideTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 120.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 16 ms ⇒ OS-provided 8.333 ms is outside 10%
                    // tolerance, snaps to supported 16.666 ms (60 Hz).
                    .timelines = CreateTimelines({10'000'000, 26'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(16.666)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToSupportedOsProvidedDisabledOrOutsideSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result =
                    "CALCULATED_SNAPPED_TO_SUPPORTED_OS_PROVIDED_DISABLED_OR_"
                    "OUTSIDE_SNAP_TOLERANCE",
                .present_deltas_difference_us = 16'000,
                .closest_supported_interval_us = 16'666,
                .deadline_derived_interval_us = 16'666,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "SnappedToEqualSupportedAndOsProvided",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 16'666'666,
            .display_supported_refresh_rates = {60.f, 120.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 16 ms ⇒ both supported 16.666 ms (60 Hz) and
                    // OS-provided 16.666 ms are within 10% tolerance and equal.
                    .timelines = CreateTimelines({10'000'000, 26'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(16.666),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(16.666)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToEqualSupportedAndOsProvided),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_SNAPPED_TO_EQUAL_SUPPORTED_AND_OS_"
                          "PROVIDED",
                .present_deltas_difference_us = 16'000,
                .closest_supported_interval_us = 16'666,
                .deadline_derived_interval_us = 16'666,
            }),
        },
        AChoreographerImplTestParams{
            .test_name =
                "SnappedToOsProvided_CloserThanSupportedWithinTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.15,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 12'500'000,
            .display_supported_refresh_rates = {90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 12 ms ⇒ both 11.111 ms (90 Hz) and 12.5 ms (80 Hz)
                    // are within 15% tolerance; snaps to closer 12.5 ms.
                    .timelines = CreateTimelines({10'000'000, 22'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(12.5),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(12.5)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToOsProvidedCloserOrTiedWithSupported),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_SNAPPED_TO_OS_PROVIDED_CLOSER_OR_"
                          "TIED_WITH_SUPPORTED",
                .present_deltas_difference_us = 12'000,
                .closest_supported_interval_us = 11'111,
                .deadline_derived_interval_us = 12'500,
            }),
        },
        AChoreographerImplTestParams{
            .test_name =
                "SnappedToSupported_CloserThanOsProvidedWithinTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.15,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 12'500'000,
            .display_supported_refresh_rates = {90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 11.5 ms ⇒ both 11.111 ms (90 Hz) and 12.5 ms (80 Hz)
                    // are within 15% tolerance; snaps to closer 11.111 ms.
                    .timelines = CreateTimelines({10'000'000, 21'500'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(12.5),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(11.111)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToSupportedCloserThanOsProvided),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_SNAPPED_TO_SUPPORTED_CLOSER_THAN_OS_"
                          "PROVIDED",
                .present_deltas_difference_us = 11'500,
                .closest_supported_interval_us = 11'111,
                .deadline_derived_interval_us = 11'111,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "SnappedToOsProvided_ExactDistanceTieWithSupported",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.15,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 12'000'000,
            .display_supported_refresh_rates = {100.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 11 ms ⇒ equidistant (1 ms) from supported 10 ms and
                    // OS-provided 12 ms; prefers OS-provided 12 ms.
                    .timelines = CreateTimelines({10'000'000, 21'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(12),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(12)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedSnappedToOsProvidedCloserOrTiedWithSupported),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_SNAPPED_TO_OS_PROVIDED_CLOSER_OR_"
                          "TIED_WITH_SUPPORTED",
                .present_deltas_difference_us = 11'000,
                .closest_supported_interval_us = 10'000,
                .deadline_derived_interval_us = 12'000,
            }),
        },
        AChoreographerImplTestParams{
            .test_name = "Unsnapped_OutsideBothSupportedAndOsProvidedTolerance",
            .derive_feature_config =
                DeriveFeatureEnabled{
                    .snap_tolerance = 0.1,
                    .snap_to_os_provided = true,
                },
            .compat33_supported = true,
            .os_provided_vsync_interval_nanos = 8'333'333,
            .display_supported_refresh_rates = {60.f, 90.f},
            .callback_data =
                FrameCallbackData{
                    .frame_time_nanos = 10'000'000,
                    // Diff 14 ms ⇒ too far from 16.666 ms, 11.111 ms, and
                    // 8.333 ms.
                    .timelines = CreateTimelines({10'000'000, 24'000'000}),
                    .preferred_index = 0,
                },
            .expected_interval = base::Milliseconds(8.333),
            .expected_deadline_derived_interval =
                ExpectIfAtLeastBaklava(base::Milliseconds(14)),
            .expected_result = ExpectResultIfAtLeastBaklava(
                kCalculatedUnsnappedOutsideSnapTolerance),
            .expected_trial_active = IsAtLeastBaklava(),
            .expected_trace = ExpectTraceIfAtLeastBaklava({
                .result = "CALCULATED_UNSNAPPED_OUTSIDE_SNAP_TOLERANCE",
                .present_deltas_difference_us = 14'000,
                .closest_supported_interval_us = 16'666,
                .deadline_derived_interval_us = 14'000,
            }),
        }),
    [](const auto& info) { return info.param.test_name; });

}  // namespace viz
