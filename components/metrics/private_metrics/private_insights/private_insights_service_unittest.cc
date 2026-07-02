// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/private_insights_service.h"

#include <atomic>

#include "base/files/scoped_temp_dir.h"
#include "base/no_destructor.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_choice_service.h"
#include "components/metrics/private_metrics/private_insights/fcp_simple_task_environment.h"
#include "components/metrics/private_metrics/private_insights/private_insights_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/federated_compute/src/fcp/client/example_query_result.pb.h"

namespace private_insights {

class PrivateInsightsServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(tmp_profile_dir_.CreateUniqueTempDir());
    mock_run_federated_computation_call_count_ = 0;
    GetLastPopulationName() = "";
    PrivateInsightsService::SetRunFederatedComputationForTesting(
        base::BindRepeating(&MockRunFederatedComputation));
    feature_list_.InitAndEnableFeatureWithParameters(
        kPrivateInsightsFeature,
        {{"fcp_server_uri", "https://example.com/test"}});
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  void TearDown() override {
    PrivateInsightsService::SetRunFederatedComputationForTesting({});
  }

  static PrivateInsightsService::FederatedComputationResult
  MockRunFederatedComputation(
      const PrivateInsightsService::FederatedComputationParams& params) {
    mock_run_federated_computation_call_count_++;
    GetLastPopulationName() = params.population_name;
    return {
        .outcome =
            PrivateInsightsService::FederatedComputationOutcome::kSuccess,
        .contributed_task_count = 1,
    };
  }

  static inline std::atomic<int> mock_run_federated_computation_call_count_ = 0;
  static std::string& GetLastPopulationName() {
    static base::NoDestructor<std::string> last_population_name;
    return *last_population_name;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir tmp_profile_dir_;
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

TEST_F(PrivateInsightsServiceTest, TriggerUploadSkipsPostingTaskWhenNoData) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);

  service.TriggerUpload();
  histogram_tester.ExpectUniqueSample(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kSkippedNoData, 1);
  EXPECT_FALSE(service.is_upload_running_);
}

TEST_F(PrivateInsightsServiceTest,
       TriggerUploadSkipsPostingTaskWhenAlreadyRunning) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);

  events::ContextualCueLogEvent event;
  service.LogContextualCueEvent(event);

  // First call: should post the task.
  service.TriggerUpload();
  histogram_tester.ExpectUniqueSample(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kTaskPosted, 1);

  // Second call: while task is running, should be skipped.
  service.TriggerUpload();
  histogram_tester.ExpectBucketCount(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kSkippedAlreadyRunning, 1);
  histogram_tester.ExpectTotalCount(kTriggerUploadOutcomeHistogram, 2);

  // Wait for task execution to complete.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 1);

  histogram_tester.ExpectTotalCount(kUploadPendingTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kUploadTimeHistogram, 1);
  histogram_tester.ExpectUniqueSample(
      kFederatedComputationOutcomeHistogram,
      PrivateInsightsService::FederatedComputationOutcome::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(kContributedTaskCountHistogram, 1, 1);

  // Third call: now that task completed, queue another event and post again.
  service.LogContextualCueEvent(event);
  service.TriggerUpload();
  histogram_tester.ExpectBucketCount(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kTaskPosted, 2);
  histogram_tester.ExpectTotalCount(kTriggerUploadOutcomeHistogram, 3);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 2);
  histogram_tester.ExpectUniqueSample(
      kFederatedComputationOutcomeHistogram,
      PrivateInsightsService::FederatedComputationOutcome::kSuccess, 2);
  histogram_tester.ExpectUniqueSample(kContributedTaskCountHistogram, 1, 2);
}

TEST_F(PrivateInsightsServiceTest, MetricsChoiceCoupling) {

  TestingPrefServiceSimple local_state;
  metrics::MetricsReportingChoiceService::RegisterPrefs(local_state.registry());
  local_state.registry()->RegisterBooleanPref(
      metrics::prefs::kMetricsReportingEnabled, false);

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(true);

  // When Init() is NOT called, UMA choice changes should be ignored.
  PrivateInsightsService uninit_service(&local_state,
                                        tmp_profile_dir_.GetPath(),
                                        test_shared_url_loader_factory_);
  EXPECT_FALSE(uninit_service.upload_timer_.IsRunning());
  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  EXPECT_FALSE(uninit_service.upload_timer_.IsRunning());

  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);

  // When Init() IS called, UMA choice changes should start/stop the service.
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  service.Init();
  EXPECT_FALSE(service.upload_timer_.IsRunning());

  // Enable UMA metrics reporting.
  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  EXPECT_TRUE(service.upload_timer_.IsRunning());

  // Disable UMA metrics reporting.
  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  EXPECT_FALSE(service.upload_timer_.IsRunning());

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(false);
}

TEST_F(PrivateInsightsServiceTest, MetricsChoiceRespectedOnStartup) {

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(true);

  // Verify choice is respected when disabled on startup.
  {
    TestingPrefServiceSimple local_state;
    metrics::MetricsReportingChoiceService::RegisterPrefs(
        local_state.registry());
    local_state.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);

    PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                   test_shared_url_loader_factory_);
    EXPECT_FALSE(service.upload_timer_.IsRunning());

    service.Init();
    EXPECT_FALSE(service.upload_timer_.IsRunning());
  }

  // Verify choice is respected when enabled on startup.
  {
    TestingPrefServiceSimple local_state;
    metrics::MetricsReportingChoiceService::RegisterPrefs(
        local_state.registry());
    local_state.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, true);

    PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                   test_shared_url_loader_factory_);
    EXPECT_FALSE(service.upload_timer_.IsRunning());

    service.Init();
    EXPECT_TRUE(service.upload_timer_.IsRunning());
  }

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(false);
}

TEST_F(PrivateInsightsServiceTest, UploadSkippedWhenServerUriEmpty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateInsightsFeature, {{"fcp_server_uri", ""}});

  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);

  events::ContextualCueLogEvent event;
  service.LogContextualCueEvent(event);

  service.TriggerUpload();

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 0);

  histogram_tester.ExpectTotalCount(kUploadPendingTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kUploadTimeHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      kFederatedComputationOutcomeHistogram,
      PrivateInsightsService::FederatedComputationOutcome::kErrorNoServerUri,
      1);
  histogram_tester.ExpectTotalCount(kContributedTaskCountHistogram, 0);
}

TEST_F(PrivateInsightsServiceTest, PopulationNameFinchParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateInsightsFeature,
      {{"fcp_server_uri", "https://example.com/test"},
       {"fcp_population_name_contextual_cues", "custom_population_name"}});

  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);

  events::ContextualCueLogEvent event;
  service.LogContextualCueEvent(event);

  service.TriggerUpload();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 1);
  EXPECT_EQ(GetLastPopulationName(), "custom_population_name");
}

TEST_F(PrivateInsightsServiceTest, LogContextualCueEvent) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateInsightsFeature, {{"max_contextual_cue_events", "2"}});

  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  EXPECT_EQ(service.contextual_cue_events_.size(), 0u);

  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  service.LogContextualCueEvent(event1);

  EXPECT_EQ(service.contextual_cue_events_.size(), 1u);
  EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_1");
  histogram_tester.ExpectBucketCount(
      kContextualCueEventsLoggingQueuedCountHistogram, 1, 1);

  events::ContextualCueLogEvent event2;
  event2.set_cue_id("cue_2");
  service.LogContextualCueEvent(event2);

  EXPECT_EQ(service.contextual_cue_events_.size(), 2u);
  EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_1");
  EXPECT_EQ(service.contextual_cue_events_[1].event.cue_id(), "cue_2");
  histogram_tester.ExpectBucketCount(
      kContextualCueEventsLoggingQueuedCountHistogram, 2, 1);
  histogram_tester.ExpectTotalCount(
      kContextualCueEventsLoggingRemovedCountHistogram, 0);

  // Logging a third event should drop the oldest event ("cue_1") due to
  // max_events=2.
  events::ContextualCueLogEvent event3;
  event3.set_cue_id("cue_3");
  service.LogContextualCueEvent(event3);

  EXPECT_EQ(service.contextual_cue_events_.size(), 2u);
  EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_2");
  EXPECT_EQ(service.contextual_cue_events_[1].event.cue_id(), "cue_3");
  histogram_tester.ExpectBucketCount(
      kContextualCueEventsLoggingQueuedCountHistogram, 2, 2);
  histogram_tester.ExpectUniqueSample(
      kContextualCueEventsLoggingRemovedCountHistogram, 1, 1);
}

TEST_F(PrivateInsightsServiceTest, SerializeEventsToQueryResult) {
  base::circular_deque<PrivateInsightsService::ContextualCueEventEntry> events;

  base::Time time1;
  ASSERT_TRUE(base::Time::FromUTCExploded({.year = 2026,
                                           .month = 6,
                                           .day_of_week = 0,
                                           .day_of_month = 28,
                                           .hour = 12,
                                           .minute = 0,
                                           .second = 0,
                                           .millisecond = 0},
                                          &time1));
  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  events.push_back({time1, event1});

  base::Time time2;
  ASSERT_TRUE(base::Time::FromUTCExploded({.year = 2026,
                                           .month = 6,
                                           .day_of_week = 0,
                                           .day_of_month = 28,
                                           .hour = 12,
                                           .minute = 5,
                                           .second = 30,
                                           .millisecond = 500},
                                          &time2));
  events::ContextualCueLogEvent event2;
  event2.set_cue_id("cue_2");
  events.push_back({time2, event2});

  fcp::client::ExampleQueryResult query_result;
  PrivateInsightsService::SerializeEventsToQueryResult(events, &query_result);

  EXPECT_EQ(query_result.result_source(),
            fcp::client::ExampleQueryResult::PRIVATE_LOGGER);
  EXPECT_EQ(query_result.stats().output_rows_count(), 2);

  const auto& vectors = query_result.vector_data().vectors();
  ASSERT_EQ(vectors.count("entry"), 1u);
  ASSERT_EQ(vectors.count("confidential_compute_event_time"), 1u);

  const auto& entry_bytes = vectors.at("entry").bytes_values();
  ASSERT_EQ(entry_bytes.value_size(), 2);
  EXPECT_EQ(entry_bytes.value(0), event1.SerializeAsString());
  EXPECT_EQ(entry_bytes.value(1), event2.SerializeAsString());

  const auto& time_strings =
      vectors.at("confidential_compute_event_time").string_values();
  ASSERT_EQ(time_strings.value_size(), 2);
  EXPECT_EQ(time_strings.value(0), "2026-06-28T12:00:00.000Z");
  EXPECT_EQ(time_strings.value(1), "2026-06-28T12:05:30.500Z");
}

TEST_F(PrivateInsightsServiceTest, RequeueEventsEmpty) {
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  service.LogContextualCueEvent(event1);
  EXPECT_EQ(service.contextual_cue_events_.size(), 1u);

  service.RequeueEvents({});
  EXPECT_EQ(service.contextual_cue_events_.size(), 1u);
  EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_1");
}

TEST_F(PrivateInsightsServiceTest, RequeueEventsPrependsRequeuedEvents) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  // Prepare pending events cue_1 and cue_2.
  base::circular_deque<PrivateInsightsService::ContextualCueEventEntry>
      pending_events;
  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  events::ContextualCueLogEvent event2;
  event2.set_cue_id("cue_2");
  pending_events.emplace_back(base::Time::Now(), event1);
  pending_events.emplace_back(base::Time::Now(), event2);

  // Log cue_3 and cue_4.
  events::ContextualCueLogEvent event3;
  event3.set_cue_id("cue_3");
  events::ContextualCueLogEvent event4;
  event4.set_cue_id("cue_4");
  service.LogContextualCueEvent(event3);
  service.LogContextualCueEvent(event4);

  // Requeuing prepends pending events before existing events.
  service.RequeueEvents(std::move(pending_events));
  EXPECT_EQ(service.contextual_cue_events_.size(), 4u);
  EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_1");
  EXPECT_EQ(service.contextual_cue_events_[1].event.cue_id(), "cue_2");
  EXPECT_EQ(service.contextual_cue_events_[2].event.cue_id(), "cue_3");
  EXPECT_EQ(service.contextual_cue_events_[3].event.cue_id(), "cue_4");

  histogram_tester.ExpectUniqueSample(
      kContextualCueEventsRequeuingRequeuedCountHistogram, 2, 1);
  histogram_tester.ExpectUniqueSample(
      kContextualCueEventsRequeuingDroppedCountHistogram, 0, 1);
}

TEST_F(PrivateInsightsServiceTest, RequeueEventsExceedsMaxEvents) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateInsightsFeature, {{"max_contextual_cue_events", "2"}});

  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  // Prepare pending events cue_1 and cue_2.
  base::circular_deque<PrivateInsightsService::ContextualCueEventEntry>
      requeue_list;
  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  events::ContextualCueLogEvent event2;
  event2.set_cue_id("cue_2");
  requeue_list.emplace_back(base::Time::Now(), event1);
  requeue_list.emplace_back(base::Time::Now(), event2);

  // Log cue_3.
  events::ContextualCueLogEvent event3;
  event3.set_cue_id("cue_3");
  service.LogContextualCueEvent(event3);

  // Oldest events should be dropped.
  service.RequeueEvents(std::move(requeue_list));
  EXPECT_EQ(service.contextual_cue_events_.size(), 2u);
  EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_2");
  EXPECT_EQ(service.contextual_cue_events_[1].event.cue_id(), "cue_3");

  histogram_tester.ExpectUniqueSample(
      kContextualCueEventsRequeuingRequeuedCountHistogram, 1, 1);
  histogram_tester.ExpectUniqueSample(
      kContextualCueEventsRequeuingDroppedCountHistogram, 1, 1);
}

struct TriggerUploadTestParams {
  const char* test_name;
  PrivateInsightsService::FederatedComputationOutcome outcome;
  bool should_requeue;
};

class PrivateInsightsServiceTriggerUploadTest
    : public PrivateInsightsServiceTest,
      public testing::WithParamInterface<TriggerUploadTestParams> {};

TEST_P(PrivateInsightsServiceTriggerUploadTest, HandleEvents) {
  const TriggerUploadTestParams& param = GetParam();

  static base::NoDestructor<fcp::client::ExampleQueryResult>
      captured_query_result;
  PrivateInsightsService::SetRunFederatedComputationForTesting(
      base::BindRepeating(
          [](PrivateInsightsService::FederatedComputationOutcome outcome,
             const PrivateInsightsService::FederatedComputationParams& params)
              -> PrivateInsightsService::FederatedComputationResult {
            *captured_query_result = params.task_env->result();
            return {
                .outcome = outcome,
                .contributed_task_count =
                    outcome == PrivateInsightsService::
                                   FederatedComputationOutcome::kSuccess
                        ? std::make_optional(1)
                        : std::nullopt,
            };
          },
          param.outcome));

  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  service.LogContextualCueEvent(event1);

  EXPECT_EQ(service.contextual_cue_events_.size(), 1u);

  service.TriggerUpload();
  EXPECT_EQ(service.contextual_cue_events_.size(), 0u);

  // Log a new event while upload is in progress.
  events::ContextualCueLogEvent event2;
  event2.set_cue_id("cue_2");
  service.LogContextualCueEvent(event2);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !service.is_upload_running_; }));

  // Verify federated computation received the queued event.
  EXPECT_EQ(captured_query_result->stats().output_rows_count(), 1);
  const auto& vectors = captured_query_result->vector_data().vectors();
  ASSERT_EQ(vectors.count("entry"), 1u);
  const auto& entry_bytes = vectors.at("entry").bytes_values();
  ASSERT_EQ(entry_bytes.value_size(), 1);
  EXPECT_EQ(entry_bytes.value(0), event1.SerializeAsString());

  if (param.should_requeue) {
    // Requeued event ("cue_1") should be placed before new event ("cue_2").
    EXPECT_EQ(service.contextual_cue_events_.size(), 2u);
    EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_1");
    EXPECT_EQ(service.contextual_cue_events_[1].event.cue_id(), "cue_2");
  } else {
    // Event is not requeued, so only the new event ("cue_2") remains in queue.
    EXPECT_EQ(service.contextual_cue_events_.size(), 1u);
    EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_2");
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivateInsightsServiceTriggerUploadTest,
    testing::Values(
        TriggerUploadTestParams{
            "Success",
            PrivateInsightsService::FederatedComputationOutcome::kSuccess,
            /*should_requeue=*/false},
        TriggerUploadTestParams{
            "Partial",
            PrivateInsightsService::FederatedComputationOutcome::kPartial,
            /*should_requeue=*/false},
        TriggerUploadTestParams{
            "Failed",
            PrivateInsightsService::FederatedComputationOutcome::kFailed,
            /*should_requeue=*/true},
        TriggerUploadTestParams{
            "ErrorOther",
            PrivateInsightsService::FederatedComputationOutcome::kErrorOther,
            /*should_requeue=*/false}),
    [](const testing::TestParamInfo<TriggerUploadTestParams>& info) {
      return info.param.test_name;
    });

}  // namespace private_insights
