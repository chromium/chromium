// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/client_info.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_upload_scheduler.h"
#include "components/metrics/test_enabled_state_provider.h"
#include "components/metrics/test_metrics_provider.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

namespace {

void YieldUntil(base::Time when) {
  while (base::Time::Now() <= when)
    base::PlatformThread::YieldCurrentThread();
}

void StoreNoClientInfoBackup(const ClientInfo& /* client_info */) {
}

std::unique_ptr<ClientInfo> ReturnNoBackup() {
  return nullptr;
}

class TestMetricsService : public MetricsService {
 public:
  TestMetricsService(MetricsStateManager* state_manager,
                     MetricsServiceClient* client,
                     PrefService* local_state)
      : MetricsService(state_manager, client, local_state) {}
  ~TestMetricsService() override {}

  using MetricsService::log_manager;
  using MetricsService::log_store;
  using MetricsService::RecordCurrentEnvironmentHelper;

  // MetricsService:
  void SetPersistentSystemProfile(const std::string& serialized_proto,
                                  bool complete) override {
    persistent_system_profile_provided_ = true;
    persistent_system_profile_complete_ = complete;
  }

  bool persistent_system_profile_provided() const {
    return persistent_system_profile_provided_;
  }
  bool persistent_system_profile_complete() const {
    return persistent_system_profile_complete_;
  }

 private:
  bool persistent_system_profile_provided_ = false;
  bool persistent_system_profile_complete_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsService);
};

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 MetricsServiceClient* client)
      : MetricsLog(client_id, session_id, MetricsLog::ONGOING_LOG, client) {}

  ~TestMetricsLog() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

class MetricsServiceTest : public testing::Test {
 public:
  MetricsServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_),
        enabled_state_provider_(new TestEnabledStateProvider(false, false)) {
    base::SetRecordActionTaskRunner(task_runner_);
    MetricsService::RegisterPrefs(testing_local_state_.registry());
  }

  ~MetricsServiceTest() override {}

  MetricsStateManager* GetMetricsStateManager() {
    // Lazy-initialize the metrics_state_manager so that it correctly reads the
    // stability state from prefs after tests have a chance to initialize it.
    if (!metrics_state_manager_) {
      metrics_state_manager_ = MetricsStateManager::Create(
          GetLocalState(), enabled_state_provider_.get(), base::string16(),
          base::Bind(&StoreNoClientInfoBackup), base::Bind(&ReturnNoBackup));
    }
    return metrics_state_manager_.get();
  }

  PrefService* GetLocalState() { return &testing_local_state_; }

  // Sets metrics reporting as enabled for testing.
  void EnableMetricsReporting() {
    enabled_state_provider_->set_consent(true);
    enabled_state_provider_->set_enabled(true);
  }

  // Finds a histogram with the specified |name_hash| in |histograms|.
  const base::HistogramBase* FindHistogram(
      const base::StatisticsRecorder::Histograms& histograms,
      uint64_t name_hash) {
    for (const base::HistogramBase* histogram : histograms) {
      if (name_hash == base::HashMetricName(histogram->histogram_name()))
        return histogram;
    }
    return nullptr;
  }

  // Checks whether |uma_log| contains any histograms that are not flagged
  // with kUmaStabilityHistogramFlag. Stability logs should only contain such
  // histograms.
  void CheckForNonStabilityHistograms(
      const ChromeUserMetricsExtension& uma_log) {
    const int kStabilityFlags = base::HistogramBase::kUmaStabilityHistogramFlag;
    const base::StatisticsRecorder::Histograms histograms =
        base::StatisticsRecorder::GetHistograms();
    for (int i = 0; i < uma_log.histogram_event_size(); ++i) {
      const uint64_t hash = uma_log.histogram_event(i).name_hash();

      const base::HistogramBase* histogram = FindHistogram(histograms, hash);
      EXPECT_TRUE(histogram) << hash;

      EXPECT_EQ(kStabilityFlags, histogram->flags() & kStabilityFlags) << hash;
    }
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<TestEnabledStateProvider> enabled_state_provider_;
  TestingPrefServiceSimple testing_local_state_;
  std::unique_ptr<MetricsStateManager> metrics_state_manager_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceTest);
};

}  // namespace

TEST_F(MetricsServiceTest, InitialStabilityLogAfterCleanShutDown) {
  EnableMetricsReporting();
  GetLocalState()->SetBoolean(prefs::kStabilityExitedCleanly, true);

  TestMetricsServiceClient client;
  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  // No initial stability log should be generated.
  EXPECT_FALSE(service.has_unsent_logs());

  // Ensure that HasPreviousSessionData() is always called on providers,
  // for consistency, even if other conditions already indicate their presence.
  EXPECT_TRUE(test_provider->has_initial_stability_metrics_called());

  // The test provider should not have been called upon to provide initial
  // stability nor regular stability metrics.
  EXPECT_FALSE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_FALSE(test_provider->provide_stability_metrics_called());
}

TEST_F(MetricsServiceTest, InitialStabilityLogAtProviderRequest) {
  EnableMetricsReporting();

  // Save an existing system profile to prefs, to correspond to what would be
  // saved from a previous session.
  TestMetricsServiceClient client;
  TestMetricsLog log("client", 1, &client);
  DelegatingProvider delegating_provider;
  TestMetricsService::RecordCurrentEnvironmentHelper(&log, GetLocalState(),
                                                     &delegating_provider);

  // Record stability build time and version from previous session, so that
  // stability metrics (including exited cleanly flag) won't be cleared.
  EnvironmentRecorder(GetLocalState())
      .SetBuildtimeAndVersion(MetricsLog::GetBuildTime(),
                              client.GetVersionString());

  // Set the clean exit flag, as that will otherwise cause a stabilty
  // log to be produced, irrespective provider requests.
  GetLocalState()->SetBoolean(prefs::kStabilityExitedCleanly, true);

  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());
  // Add a metrics provider that requests a stability log.
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  test_provider->set_has_initial_stability_metrics(true);
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  // The initial stability log should be generated and persisted in unsent logs.
  MetricsLogStore* log_store = service.log_store();
  EXPECT_TRUE(log_store->has_unsent_logs());
  EXPECT_FALSE(log_store->has_staged_log());

  // Ensure that HasPreviousSessionData() is always called on providers,
  // for consistency, even if other conditions already indicate their presence.
  EXPECT_TRUE(test_provider->has_initial_stability_metrics_called());

  // The test provider should have been called upon to provide initial
  // stability and regular stability metrics.
  EXPECT_TRUE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());

  // Stage the log and retrieve it.
  log_store->StageNextLog();
  EXPECT_TRUE(log_store->has_staged_log());

  std::string uncompressed_log;
  EXPECT_TRUE(
      compression::GzipUncompress(log_store->staged_log(), &uncompressed_log));

  ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(uma_log.ParseFromString(uncompressed_log));

  EXPECT_TRUE(uma_log.has_client_id());
  EXPECT_TRUE(uma_log.has_session_id());
  EXPECT_TRUE(uma_log.has_system_profile());
  EXPECT_EQ(0, uma_log.user_action_event_size());
  EXPECT_EQ(0, uma_log.omnibox_event_size());
  EXPECT_EQ(0, uma_log.perf_data_size());
  CheckForNonStabilityHistograms(uma_log);

  // As there wasn't an unclean shutdown, this log has zero crash count.
  EXPECT_EQ(0, uma_log.system_profile().stability().crash_count());
}

TEST_F(MetricsServiceTest, InitialStabilityLogAfterCrash) {
  EnableMetricsReporting();
  GetLocalState()->ClearPref(prefs::kStabilityExitedCleanly);

  // Set up prefs to simulate restarting after a crash.

  // Save an existing system profile to prefs, to correspond to what would be
  // saved from a previous session.
  TestMetricsServiceClient client;
  TestMetricsLog log("client", 1, &client);
  DelegatingProvider delegating_provider;
  TestMetricsService::RecordCurrentEnvironmentHelper(&log, GetLocalState(),
                                                     &delegating_provider);

  // Record stability build time and version from previous session, so that
  // stability metrics (including exited cleanly flag) won't be cleared.
  EnvironmentRecorder(GetLocalState())
      .SetBuildtimeAndVersion(MetricsLog::GetBuildTime(),
                              client.GetVersionString());

  GetLocalState()->SetBoolean(prefs::kStabilityExitedCleanly, false);

  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());
  // Add a provider.
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));
  service.InitializeMetricsRecordingState();

  // The initial stability log should be generated and persisted in unsent logs.
  MetricsLogStore* log_store = service.log_store();
  EXPECT_TRUE(log_store->has_unsent_logs());
  EXPECT_FALSE(log_store->has_staged_log());

  // Ensure that HasPreviousSessionData() is always called on providers,
  // for consistency, even if other conditions already indicate their presence.
  EXPECT_TRUE(test_provider->has_initial_stability_metrics_called());

  // The test provider should have been called upon to provide initial
  // stability and regular stability metrics.
  EXPECT_TRUE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());

  // Stage the log and retrieve it.
  log_store->StageNextLog();
  EXPECT_TRUE(log_store->has_staged_log());

  std::string uncompressed_log;
  EXPECT_TRUE(
      compression::GzipUncompress(log_store->staged_log(), &uncompressed_log));

  ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(uma_log.ParseFromString(uncompressed_log));

  EXPECT_TRUE(uma_log.has_client_id());
  EXPECT_TRUE(uma_log.has_session_id());
  EXPECT_TRUE(uma_log.has_system_profile());
  EXPECT_EQ(0, uma_log.user_action_event_size());
  EXPECT_EQ(0, uma_log.omnibox_event_size());
  EXPECT_EQ(0, uma_log.perf_data_size());
  CheckForNonStabilityHistograms(uma_log);

  EXPECT_EQ(1, uma_log.system_profile().stability().crash_count());
}

TEST_F(MetricsServiceTest,
       MetricsProviderOnRecordingDisabledCalledOnInitialStop) {
  TestMetricsServiceClient client;
  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();
  service.Stop();

  EXPECT_TRUE(test_provider->on_recording_disabled_called());
}

TEST_F(MetricsServiceTest, MetricsProvidersInitialized) {
  TestMetricsServiceClient client;
  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  EXPECT_TRUE(test_provider->init_called());
}

TEST_F(MetricsServiceTest, SystemProfileDataProvidedOnEnableRecording) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  // ProvideSystemProfileMetrics() shouldn't be called initially.
  EXPECT_FALSE(test_provider->provide_system_profile_metrics_called());
  EXPECT_FALSE(service.persistent_system_profile_provided());

  service.Start();

  // Start should call ProvideSystemProfileMetrics().
  EXPECT_TRUE(test_provider->provide_system_profile_metrics_called());
  EXPECT_TRUE(service.persistent_system_profile_provided());
  EXPECT_FALSE(service.persistent_system_profile_complete());
}

TEST_F(MetricsServiceTest, SplitRotation) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());
  service.InitializeMetricsRecordingState();
  service.Start();
  // Rotation loop should create a log and mark state as idle.
  // Upload loop should start upload or be restarted.
  // The independent-metrics upload job will be started and always be a task.
  task_runner_->RunPendingTasks();
  // Rotation loop should terminated due to being idle.
  // Upload loop should start uploading if it isn't already.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  service.OnApplicationNotIdle();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  // Log generation should be suppressed due to unsent log.
  // Idle state should not be reset.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  // Make sure idle state was not reset.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  // Upload should not be rescheduled, since there are no other logs.
  client.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client.uploader()->is_uploading());
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  // Running should generate a log, restart upload loop, and mark idle.
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(client.uploader()->is_uploading());
  EXPECT_EQ(3U, task_runner_->NumPendingTasks());
  // Upload should start, and rotation loop should idle out.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  // Uploader should reschedule when there is another log available.
  service.PushExternalLog("Blah");
  client.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client.uploader()->is_uploading());
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  // Upload should start.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
}

TEST_F(MetricsServiceTest, LastLiveTimestamp) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  base::Time initial_last_live_time =
      GetLocalState()->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp);

  service.InitializeMetricsRecordingState();
  service.Start();

  task_runner_->RunPendingTasks();
  size_t num_pending_tasks = task_runner_->NumPendingTasks();

  service.StartUpdatingLastLiveTimestamp();

  // Starting the update sequence should not write anything, but should
  // set up for a later write.
  EXPECT_EQ(
      initial_last_live_time,
      GetLocalState()->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp));
  EXPECT_EQ(num_pending_tasks + 1, task_runner_->NumPendingTasks());

  // To avoid flakiness, yield until we're over a microsecond threshold.
  YieldUntil(initial_last_live_time + base::TimeDelta::FromMicroseconds(2));

  task_runner_->RunPendingTasks();

  // Verify that the time has updated in local state.
  base::Time updated_last_live_time =
      GetLocalState()->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp);
  EXPECT_LT(initial_last_live_time, updated_last_live_time);

  // Double check that an update schedules again...
  YieldUntil(updated_last_live_time + base::TimeDelta::FromMicroseconds(2));

  task_runner_->RunPendingTasks();
  EXPECT_LT(
      updated_last_live_time,
      GetLocalState()->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp));
}

}  // namespace metrics
