// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time_override.h"
#include "components/metrics/dwa/dwa_entry_builder.h"
#include "components/metrics/dwa/dwa_pref_names.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::dwa {
class DwaServiceTest : public testing::Test {
 public:
  DwaServiceTest() {
    DwaRecorder::Get()->Purge();

    MetricsStateManager::RegisterPrefs(prefs_.registry());
    DwaService::RegisterPrefs(prefs_.registry());

    scoped_feature_list_.InitAndEnableFeature(kDwaFeature);
  }

  DwaServiceTest(const DwaServiceTest&) = delete;
  DwaServiceTest& operator=(const DwaServiceTest&) = delete;

  ~DwaServiceTest() override = default;

  void TearDown() override { DwaRecorder::Get()->Purge(); }

  int GetPersistedLogCount() {
    return prefs_.GetList(prefs::kUnsentLogStoreName).size();
  }

 protected:
  // Check that the values in |coarse_system_info| are filled in and expected
  // ones correspond to the test data.
  void CheckCoarseSystemInformation(
      const ::dwa::CoarseSystemInfo& coarse_system_info) {
    EXPECT_TRUE(coarse_system_info.has_channel());
    // TestMetricsServiceClient::GetChannel() returns CHANNEL_BETA, so we should
    // expect |coarse_system_info| channel to be "NOT_STABLE".
    EXPECT_EQ(coarse_system_info.channel(),
              ::dwa::CoarseSystemInfo::CHANNEL_NOT_STABLE);
    EXPECT_TRUE(coarse_system_info.has_platform());
    EXPECT_TRUE(coarse_system_info.has_geo_designation());
    EXPECT_TRUE(coarse_system_info.has_client_age());
    EXPECT_TRUE(coarse_system_info.has_milestone_prefix_trimmed());
    EXPECT_TRUE(coarse_system_info.has_is_ukm_enabled());
  }

  // Records a test data DWA metric into DwaRecorder.
  void RecordTestMetric() {
    ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
    builder.SetContent("adtech.com");
    builder.SetMetric("Length", 5);
    builder.Record(DwaRecorder::Get());
  }

  TestMetricsServiceClient client_;
  TestingPrefServiceSimple prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DwaServiceEnvironmentTest : public DwaServiceTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(DwaServiceTest, RecordCoarseSystemInformation) {
  TestingPrefServiceSimple pref_service;
  MetricsStateManager::RegisterPrefs(pref_service.registry());
  ::dwa::CoarseSystemInfo coarse_system_info;
  DwaService::RecordCoarseSystemInformation(client_, pref_service,
                                            &coarse_system_info);
  CheckCoarseSystemInformation(coarse_system_info);
}

TEST_F(DwaServiceTest, ClientIdIsGenerated) {
  TestingPrefServiceSimple pref_service;
  DwaService::RegisterPrefs(pref_service.registry());

  base::Time expected_time_at_daily_resolution;
  EXPECT_TRUE(base::Time::FromString("1 May 2024 00:00:00 GMT",
                                     &expected_time_at_daily_resolution));

  uint64_t client_id;

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("1 May 2024 12:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id = DwaService::GetEphemeralClientId(pref_service);
  }

  EXPECT_GT(pref_service.GetUint64(metrics::dwa::prefs::kDwaClientId), 0u);
  EXPECT_EQ(client_id,
            pref_service.GetUint64(metrics::dwa::prefs::kDwaClientId));
  EXPECT_EQ(expected_time_at_daily_resolution,
            pref_service.GetTime(metrics::dwa::prefs::kDwaClientIdLastUpdated));
}

TEST_F(DwaServiceTest, ClientIdOnlyChangesBetweenDays) {
  TestingPrefServiceSimple pref_service;
  DwaService::RegisterPrefs(pref_service.registry());

  uint64_t client_id_day1;
  uint64_t client_id_day2;
  uint64_t client_id_day2_later;

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("1 May 2024 12:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id_day1 = DwaService::GetEphemeralClientId(pref_service);
  }

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("2 May 2024 12:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id_day2 = DwaService::GetEphemeralClientId(pref_service);
  }

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("2 May 2024 16:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id_day2_later = DwaService::GetEphemeralClientId(pref_service);
  }

  EXPECT_NE(client_id_day1, client_id_day2);
  EXPECT_EQ(client_id_day2, client_id_day2_later);
}

TEST_F(DwaServiceEnvironmentTest, Flush) {
  DwaService service(&client_, &prefs_);
  DwaRecorder::Get()->EnableRecording();

  // Tests Flush() when there are no page load events.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(DwaRecorder::Get()->HasPageLoadEvents());

  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  EXPECT_EQ(GetPersistedLogCount(), 0);

  // Tests Flush() when there are page load events.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  DwaRecorder::Get()->OnPageLoad();
  EXPECT_TRUE(DwaRecorder::Get()->HasPageLoadEvents());

  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  EXPECT_EQ(GetPersistedLogCount(), 1);
}

TEST_F(DwaServiceEnvironmentTest, Purge) {
  DwaService service(&client_, &prefs_);
  DwaRecorder::Get()->EnableRecording();

  // Test that Purge() removes all metrics (entries and page load events).
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  DwaRecorder::Get()->OnPageLoad();
  EXPECT_TRUE(DwaRecorder::Get()->HasPageLoadEvents());
  RecordTestMetric();
  service.Purge();
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(DwaRecorder::Get()->HasPageLoadEvents());

  // Test that Purge() removes all logs.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  DwaRecorder::Get()->OnPageLoad();
  EXPECT_TRUE(DwaRecorder::Get()->HasPageLoadEvents());

  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  service.Purge();
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(DwaRecorder::Get()->HasPageLoadEvents());
  EXPECT_EQ(GetPersistedLogCount(), 0);
}

TEST_F(DwaServiceEnvironmentTest, EnableDisableRecordingAndReporting) {
  DwaService service(&client_, &prefs_);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // When the reporting is enabled, the scheduler starts and creates tasks to
  // rotate logs.
  service.EnableReporting();
  EXPECT_GE(task_environment_.GetPendingMainThreadTaskCount(), 1u);

  service.DisableReporting();
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Tests EnableRecording() records an entry.
  DwaRecorder::Get()->EnableRecording();
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());

  // Save the above entry to memory as a page load event.
  DwaRecorder::Get()->OnPageLoad();
  EXPECT_TRUE(DwaRecorder::Get()->HasPageLoadEvents());
  EXPECT_EQ(GetPersistedLogCount(), 0);
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());

  // DisableRecording() and persist the above page load event to disk.
  DwaRecorder::Get()->DisableRecording();
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  EXPECT_EQ(GetPersistedLogCount(), 1);
  EXPECT_TRUE(service.unsent_log_store()->has_unsent_logs());

  // Tests EnableRecording() with reporting_active() also starts
  // DwaReportingService when there are unsent logs on disk.
  service.EnableReporting();
  DwaRecorder::Get()->EnableRecording();
  // Validate there should be two pending tasks, one to rotate logs, and one to
  // upload unsent logs from disk.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 2u);

  base::TimeDelta upload_interval = client_.GetUploadInterval();
  task_environment_.FastForwardBy(upload_interval);
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Simulate logs upload.
  client_.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client_.uploader()->is_uploading());

  // Logs are uploaded and there are no metrics in memory or on disk.
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(DwaRecorder::Get()->HasPageLoadEvents());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());
  EXPECT_EQ(GetPersistedLogCount(), 0);

  // Cleans up state of DwaService for the test following.
  service.DisableReporting();
  service.Purge();

  // Tests DisableRecording() does not record an entry.
  DwaRecorder::Get()->DisableRecording();
  RecordTestMetric();
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
}

TEST_F(DwaServiceEnvironmentTest, LogsRotatedPeriodically) {
  DwaService service(&client_, &prefs_);
  DwaRecorder::Get()->EnableRecording();
  service.EnableReporting();

  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  DwaRecorder::Get()->OnPageLoad();

  // Metrics are stored in memory as page load events in DwaRecorder, and there
  // are no unsent logs.
  EXPECT_TRUE(DwaRecorder::Get()->HasPageLoadEvents());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());

  // Metrics are stored in memory as unsent logs, and DwaRecorder is empty.
  base::TimeDelta upload_interval = client_.GetUploadInterval();
  task_environment_.FastForwardBy(upload_interval);
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(DwaRecorder::Get()->HasPageLoadEvents());
  EXPECT_TRUE(service.unsent_log_store()->has_unsent_logs());
  // PersistedLogCount is 0 because logs are stored in memory and not on disk.
  EXPECT_EQ(GetPersistedLogCount(), 0);
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Simulate logs upload.
  client_.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client_.uploader()->is_uploading());

  // Logs are uploaded and there are no metrics in memory or on disk.
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(DwaRecorder::Get()->HasPageLoadEvents());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());
  EXPECT_EQ(GetPersistedLogCount(), 0);

  // Checks there is another rotation scheduled when the previous one finished.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1u);

  // Repeat test above to validate the task scheduled is the task that rotates
  // logs.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  DwaRecorder::Get()->OnPageLoad();

  EXPECT_TRUE(DwaRecorder::Get()->HasPageLoadEvents());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());

  task_environment_.FastForwardBy(upload_interval);
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(DwaRecorder::Get()->HasPageLoadEvents());
  EXPECT_TRUE(service.unsent_log_store()->has_unsent_logs());
  EXPECT_EQ(GetPersistedLogCount(), 0);
  EXPECT_TRUE(client_.uploader()->is_uploading());

  client_.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client_.uploader()->is_uploading());

  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(DwaRecorder::Get()->HasPageLoadEvents());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());
  EXPECT_EQ(GetPersistedLogCount(), 0);
}

}  // namespace metrics::dwa
