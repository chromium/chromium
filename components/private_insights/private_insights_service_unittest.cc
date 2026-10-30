// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/private_insights_service.h"

#include <atomic>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/no_destructor.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_choice_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/private_insights/fcp_simple_task_environment.h"
#include "components/private_insights/private_insights_features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/federated_compute/src/fcp/client/example_query_result.pb.h"

namespace private_insights {

// Shared fixture state. Tests derive from one of the parameterized fixtures
// below so that every upload scenario runs against both the original upload
// path and the `kPrivateInsightsUploadTaskPipeline` path.
class PrivateInsightsServiceTestBase : public testing::Test {
 protected:
  void SetUpWithUploadTaskPipeline(bool upload_task_pipeline_enabled) {
    ASSERT_TRUE(tmp_profile_dir_.CreateUniqueTempDir());
    mock_run_federated_computation_call_count_ = 0;
    GetLastPopulationName() = "";
    GetLastServerUri() = "";
    PrivateInsightsService::SetRunFederatedComputationForTesting(
        base::BindRepeating(&MockRunFederatedComputation));
    std::vector<base::test::FeatureRefAndParams> enabled = {
        {kPrivateInsightsFeature,
         {{"fcp_server_uri", "https://example.com/test"}}}};
    std::vector<base::test::FeatureRef> disabled;
    if (upload_task_pipeline_enabled) {
      enabled.push_back({kPrivateInsightsUploadTaskPipeline, {}});
    } else {
      disabled.push_back(kPrivateInsightsUploadTaskPipeline);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
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
    GetLastServerUri() = params.federated_service_uri;
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
  static std::string& GetLastServerUri() {
    static base::NoDestructor<std::string> last_server_uri;
    return *last_server_uri;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir tmp_profile_dir_;
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

std::string UploadPathName(bool upload_task_pipeline_enabled) {
  return upload_task_pipeline_enabled ? "UploadTaskPipeline"
                                      : "OriginalPipeline";
}

// Parameterized over whether `kPrivateInsightsUploadTaskPipeline` is enabled.
class PrivateInsightsServiceTest : public PrivateInsightsServiceTestBase,
                                   public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override { SetUpWithUploadTaskPipeline(GetParam()); }
  bool upload_task_pipeline_enabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrivateInsightsServiceTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return UploadPathName(info.param);
                         });

TEST_P(PrivateInsightsServiceTest, TriggerUploadSkipsPostingTaskWhenNoData) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  service.TriggerUpload();
  histogram_tester.ExpectUniqueSample(
      kTriggerUploadOutcomeHistogram,
      PrivateInsightsService::TriggerUploadOutcome::kSkippedNoData, 1);
  EXPECT_FALSE(service.is_upload_running_);
}

TEST_P(PrivateInsightsServiceTest,
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
  ASSERT_TRUE(base::test::RunUntil([&]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    return !service.is_upload_running_;
  }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 1);

  histogram_tester.ExpectTotalCount(kUploadPendingTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kUploadTimeHistogram, 1);
  histogram_tester.ExpectTotalCount(kUploadTime2Histogram, 1);
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

  ASSERT_TRUE(base::test::RunUntil([&]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    return !service.is_upload_running_;
  }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 2);
  histogram_tester.ExpectUniqueSample(
      kFederatedComputationOutcomeHistogram,
      PrivateInsightsService::FederatedComputationOutcome::kSuccess, 2);
  histogram_tester.ExpectUniqueSample(kContributedTaskCountHistogram, 1, 2);
}

TEST_P(PrivateInsightsServiceTest, ShutdownLogsHistogram) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);

  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  service.LogContextualCueEvent(event1);

  events::ContextualCueLogEvent event2;
  event2.set_cue_id("cue_2");
  service.LogContextualCueEvent(event2);

  service.Shutdown();

  histogram_tester.ExpectUniqueSample(
      kContextualCueEventsLoggingOnShutdownCountHistogram, 2, 1);
}

TEST_P(PrivateInsightsServiceTest, MetricsChoiceCoupling) {
  TestingPrefServiceSimple local_state;
  local_state.registry()->RegisterBooleanPref(
      metrics::prefs::kMetricsReportingEnabled, false);

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(true);

  // When Init() is NOT called, UMA choice changes should be ignored.
  PrivateInsightsService uninit_service(&local_state,
                                        tmp_profile_dir_.GetPath(),
                                        test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(uninit_service.sequence_checker_);
  EXPECT_FALSE(uninit_service.upload_timer_.IsRunning());
  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  EXPECT_FALSE(uninit_service.upload_timer_.IsRunning());

  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);

  // When Init() IS called, UMA choice changes should start/stop the service.
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
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

TEST_P(PrivateInsightsServiceTest, MetricsChoiceRespectedOnStartup) {
  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(true);

  // Verify choice is respected when disabled on startup.
  {
    TestingPrefServiceSimple local_state;
    local_state.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);

    PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                   test_shared_url_loader_factory_);
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    EXPECT_FALSE(service.upload_timer_.IsRunning());

    service.Init();
    EXPECT_FALSE(service.upload_timer_.IsRunning());
  }

  // Verify choice is respected when enabled on startup.
  {
    TestingPrefServiceSimple local_state;
    local_state.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, true);

    PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                   test_shared_url_loader_factory_);
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    EXPECT_FALSE(service.upload_timer_.IsRunning());

    service.Init();
    EXPECT_TRUE(service.upload_timer_.IsRunning());
  }

  PrivateInsightsMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookupForTesting(false);
}

TEST_P(PrivateInsightsServiceTest, UploadSkippedWhenServerUriEmpty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateInsightsFeature, {{"fcp_server_uri", ""}});

  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  events::ContextualCueLogEvent event;
  service.LogContextualCueEvent(event);

  service.TriggerUpload();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    return !service.is_upload_running_;
  }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 0);

  if (upload_task_pipeline_enabled()) {
    // The UploadTask pipeline validates the server URI before posting, so
    // nothing reaches the thread pool.
    histogram_tester.ExpectUniqueSample(
        kTriggerUploadOutcomeHistogram,
        PrivateInsightsService::TriggerUploadOutcome::kSkippedNoServerUri, 1);
    histogram_tester.ExpectTotalCount(kUploadPendingTimeHistogram, 0);
  } else {
    histogram_tester.ExpectUniqueSample(
        kTriggerUploadOutcomeHistogram,
        PrivateInsightsService::TriggerUploadOutcome::kTaskPosted, 1);
    histogram_tester.ExpectTotalCount(kUploadPendingTimeHistogram, 1);
  }
  histogram_tester.ExpectTotalCount(kUploadTimeHistogram, 0);
  histogram_tester.ExpectTotalCount(kUploadTime2Histogram, 0);
  histogram_tester.ExpectUniqueSample(
      kFederatedComputationOutcomeHistogram,
      PrivateInsightsService::FederatedComputationOutcome::kErrorNoServerUri,
      1);
  histogram_tester.ExpectTotalCount(kContributedTaskCountHistogram, 0);
  // Events are dropped, not requeued, on both paths.
  EXPECT_TRUE(service.contextual_cue_events_.empty());
}

TEST_P(PrivateInsightsServiceTest, PopulationNameFinchParam) {
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
  ASSERT_TRUE(base::test::RunUntil([&]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    return !service.is_upload_running_;
  }));
  EXPECT_EQ(mock_run_federated_computation_call_count_, 1);
  EXPECT_EQ(GetLastPopulationName(), "custom_population_name");
  EXPECT_EQ(GetLastServerUri(), "https://example.com/test");
}

TEST_P(PrivateInsightsServiceTest, LogContextualCueEvent) {
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

TEST_P(PrivateInsightsServiceTest, SerializeEventsToQueryResult) {
  // Capture the query result that TriggerUpload() serialized into the FCP task
  // environment. The callback runs on a ThreadPool thread, but the test waits
  // for the upload to finish before reading, so this local is safe to capture
  // by reference. The fixture's TearDown() resets the testing callback.
  fcp::client::ExampleQueryResult captured_query_result;
  PrivateInsightsService::SetRunFederatedComputationForTesting(
      base::BindLambdaForTesting(
          [&](const PrivateInsightsService::FederatedComputationParams& params)
              -> PrivateInsightsService::FederatedComputationResult {
            captured_query_result = params.task_env->result();
            return {
                .outcome = PrivateInsightsService::FederatedComputationOutcome::
                    kSuccess,
                .contributed_task_count = 1,
            };
          }));

  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  const base::Time time1 = base::Time::Now();
  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  service.LogContextualCueEvent(event1);

  task_environment_.FastForwardBy(base::Minutes(5) + base::Milliseconds(500));
  const base::Time time2 = base::Time::Now();
  events::ContextualCueLogEvent event2;
  event2.set_cue_id("cue_2");
  service.LogContextualCueEvent(event2);

  service.TriggerUpload();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    return !service.is_upload_running_;
  }));

  const fcp::client::ExampleQueryResult& query_result = captured_query_result;
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
  EXPECT_EQ(time_strings.value(0), base::TimeFormatAsIso8601(time1));
  EXPECT_EQ(time_strings.value(1), base::TimeFormatAsIso8601(time2));
}

// Helper: makes the mocked federated computation report `outcome`.
void SetMockOutcome(
    PrivateInsightsService::FederatedComputationOutcome outcome) {
  PrivateInsightsService::SetRunFederatedComputationForTesting(
      base::BindRepeating(
          [](PrivateInsightsService::FederatedComputationOutcome outcome,
             const PrivateInsightsService::FederatedComputationParams&)
              -> PrivateInsightsService::FederatedComputationResult {
            return {.outcome = outcome, .contributed_task_count = std::nullopt};
          },
          outcome));
}

TEST_P(PrivateInsightsServiceTest, RequeueEventsEmpty) {
  SetMockOutcome(PrivateInsightsService::FederatedComputationOutcome::kFailed);
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  // Nothing queued: nothing is uploaded, so nothing can be requeued.
  service.TriggerUpload();
  EXPECT_FALSE(service.is_upload_running_);

  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  service.LogContextualCueEvent(event1);
  EXPECT_EQ(service.contextual_cue_events_.size(), 1u);
  EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_1");
  histogram_tester.ExpectTotalCount(
      kContextualCueEventsRequeuingRequeuedCountHistogram, 0);
}

TEST_P(PrivateInsightsServiceTest, RequeueEventsPrependsRequeuedEvents) {
  SetMockOutcome(PrivateInsightsService::FederatedComputationOutcome::kFailed);
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  // Log cue_1 and cue_2 and start an upload that will fail.
  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  events::ContextualCueLogEvent event2;
  event2.set_cue_id("cue_2");
  service.LogContextualCueEvent(event1);
  service.LogContextualCueEvent(event2);
  service.TriggerUpload();
  ASSERT_TRUE(service.contextual_cue_events_.empty());

  // Log cue_3 and cue_4 while the upload is in flight.
  events::ContextualCueLogEvent event3;
  event3.set_cue_id("cue_3");
  events::ContextualCueLogEvent event4;
  event4.set_cue_id("cue_4");
  service.LogContextualCueEvent(event3);
  service.LogContextualCueEvent(event4);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    return !service.is_upload_running_;
  }));

  // A failed upload requeues pending events before existing events.
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

TEST_P(PrivateInsightsServiceTest, RequeueEventsExceedsMaxEvents) {
  SetMockOutcome(PrivateInsightsService::FederatedComputationOutcome::kFailed);
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kPrivateInsightsFeature, {{"fcp_server_uri", "https://example.com/test"},
                                {"max_contextual_cue_events", "2"}});

  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  // Log cue_1 and cue_2 and start an upload that will fail.
  events::ContextualCueLogEvent event1;
  event1.set_cue_id("cue_1");
  events::ContextualCueLogEvent event2;
  event2.set_cue_id("cue_2");
  service.LogContextualCueEvent(event1);
  service.LogContextualCueEvent(event2);
  service.TriggerUpload();
  ASSERT_TRUE(service.contextual_cue_events_.empty());

  // Log cue_3 while the upload is in flight.
  events::ContextualCueLogEvent event3;
  event3.set_cue_id("cue_3");
  service.LogContextualCueEvent(event3);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    return !service.is_upload_running_;
  }));

  // A failed upload requeues; events that don't fit under max_events (2) are
  // dropped, oldest first.
  EXPECT_EQ(service.contextual_cue_events_.size(), 2u);
  EXPECT_EQ(service.contextual_cue_events_[0].event.cue_id(), "cue_2");
  EXPECT_EQ(service.contextual_cue_events_[1].event.cue_id(), "cue_3");

  histogram_tester.ExpectUniqueSample(
      kContextualCueEventsRequeuingRequeuedCountHistogram, 1, 1);
  histogram_tester.ExpectUniqueSample(
      kContextualCueEventsRequeuingDroppedCountHistogram, 1, 1);
}

// Verifies which upload path the flag selects. The original path serializes on
// the main sequence synchronously inside TriggerUpload(); the UploadTask
// pipeline defers serialization to the thread pool. Both must hand the same
// serialized events to the federated computation.
TEST_P(PrivateInsightsServiceTest, UploadPathSelectedByFlag) {
  fcp::client::ExampleQueryResult captured_query_result;
  PrivateInsightsService::SetRunFederatedComputationForTesting(
      base::BindLambdaForTesting(
          [&](const PrivateInsightsService::FederatedComputationParams& params)
              -> PrivateInsightsService::FederatedComputationResult {
            captured_query_result = params.task_env->result();
            return {
                .outcome = PrivateInsightsService::FederatedComputationOutcome::
                    kSuccess,
                .contributed_task_count = 1,
            };
          }));

  TestingPrefServiceSimple local_state;
  PrivateInsightsService service(&local_state, tmp_profile_dir_.GetPath(),
                                 test_shared_url_loader_factory_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);

  events::ContextualCueLogEvent event;
  event.set_cue_id("cue_1");
  service.LogContextualCueEvent(event);
  ASSERT_EQ(service.fcp_task_env_->result().stats().output_rows_count(), 0);

  service.TriggerUpload();
  if (!upload_task_pipeline_enabled()) {
    // Original path: events are already serialized into the task environment
    // on the main sequence by the time TriggerUpload() returns.
    EXPECT_EQ(service.fcp_task_env_->result().stats().output_rows_count(), 1);
  }
  // (For the UploadTask pipeline, serialization happens on the thread pool,
  // so its timing relative to this point is not deterministic; it is verified
  // via `captured_query_result` below instead.)

  ASSERT_TRUE(base::test::RunUntil([&]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    return !service.is_upload_running_;
  }));

  EXPECT_EQ(captured_query_result.stats().output_rows_count(), 1);
  const auto& entry_bytes =
      captured_query_result.vector_data().vectors().at("entry").bytes_values();
  ASSERT_EQ(entry_bytes.value_size(), 1);
  EXPECT_EQ(entry_bytes.value(0), event.SerializeAsString());
}

struct TriggerUploadTestParams {
  const char* test_name;
  PrivateInsightsService::FederatedComputationOutcome outcome;
  bool should_requeue;
};

// Parameterized over (upload path, federated computation outcome).
class PrivateInsightsServiceTriggerUploadTest
    : public PrivateInsightsServiceTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, TriggerUploadTestParams>> {
 protected:
  void SetUp() override {
    SetUpWithUploadTaskPipeline(std::get<0>(GetParam()));
  }
};

TEST_P(PrivateInsightsServiceTriggerUploadTest, HandleEvents) {
  const TriggerUploadTestParams& param = std::get<1>(GetParam());

  fcp::client::ExampleQueryResult captured_query_result;
  PrivateInsightsService::SetRunFederatedComputationForTesting(
      base::BindLambdaForTesting(
          [&](const PrivateInsightsService::FederatedComputationParams& params)
              -> PrivateInsightsService::FederatedComputationResult {
            captured_query_result = params.task_env->result();
            return {
                .outcome = param.outcome,
                .contributed_task_count =
                    param.outcome == PrivateInsightsService::
                                         FederatedComputationOutcome::kSuccess
                        ? std::make_optional(1)
                        : std::nullopt,
            };
          }));

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

  ASSERT_TRUE(base::test::RunUntil([&]() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(service.sequence_checker_);
    return !service.is_upload_running_;
  }));

  // Verify federated computation received the queued event.
  EXPECT_EQ(captured_query_result.stats().output_rows_count(), 1);
  const auto& vectors = captured_query_result.vector_data().vectors();
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
    testing::Combine(
        testing::Bool(),
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
                PrivateInsightsService::FederatedComputationOutcome::
                    kErrorOther,
                /*should_requeue=*/false})),
    [](const testing::TestParamInfo<std::tuple<bool, TriggerUploadTestParams>>&
           info) {
      return UploadPathName(std::get<0>(info.param)) + "_" +
             std::get<1>(info.param).test_name;
    });

}  // namespace private_insights
