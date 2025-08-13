// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/reporting_service.h"

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_scheduler.h"
#include "components/metrics/metrics_upload_scheduler.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

namespace {

// Represent a flushed log and its metadata to be used for testing.
struct TestLog {
  explicit TestLog(const std::string& log) : log(log), user_id(std::nullopt) {}
  TestLog(const std::string& log, uint64_t user_id)
      : log(log), user_id(user_id) {}
  TestLog(const std::string& log, uint64_t user_id, LogMetadata log_metadata)
      : log(log), user_id(user_id), log_metadata(log_metadata) {}
  TestLog(const TestLog& other) = default;
  ~TestLog() = default;

  const std::string log;
  const std::optional<uint64_t> user_id;
  const LogMetadata log_metadata;
};

const char kTestUploadUrl[] = "test_url";
const char kTestMimeType[] = "test_mime_type";

class TestLogStore : public LogStore {
 public:
  TestLogStore() = default;
  ~TestLogStore() override = default;

  void AddLog(const TestLog& log) { logs_.push_back(log); }

  // LogStore:
  bool has_unsent_logs() const override { return !logs_.empty(); }
  bool has_staged_log() const override { return !staged_log_hash_.empty(); }
  const std::string& staged_log() const override { return logs_.front().log; }
  const std::string& staged_log_hash() const override {
    return staged_log_hash_;
  }
  std::optional<uint64_t> staged_log_user_id() const override {
    return logs_.front().user_id;
  }
  const LogMetadata staged_log_metadata() const override {
    return logs_.front().log_metadata;
  }
  const std::string& staged_log_signature() const override {
    return base::EmptyString();
  }
  void StageNextLog() override {
    if (has_unsent_logs()) {
      staged_log_hash_ = base::SHA1HashString(logs_.front().log);
    }
  }
  void DiscardStagedLog(std::string_view reason) override {
    if (!has_staged_log()) {
      return;
    }
    logs_.pop_front();
    staged_log_hash_.clear();
  }
  void MarkStagedLogAsSent() override {}
  void TrimAndPersistUnsentLogs(bool overwrite_in_memory_store) override {}
  void LoadPersistedUnsentLogs() override {}

 private:
  std::string staged_log_hash_;
  std::deque<TestLog> logs_;
};

class TestReportingService : public ReportingService {
 public:
  TestReportingService(MetricsServiceClient* client, PrefService* local_state)
      : ReportingService(client,
                         local_state,
                         100,
                         /*logs_event_manager=*/nullptr) {
    Initialize();
  }

  TestReportingService(const TestReportingService&) = delete;
  TestReportingService& operator=(const TestReportingService&) = delete;

  ~TestReportingService() override = default;

  void AddLog(const TestLog& log) { log_store_.AddLog(log); }
  bool HasUnsentLogs() { return log_store_.has_unsent_logs(); }

 private:
  // ReportingService:
  LogStore* log_store() override { return &log_store_; }
  GURL GetUploadUrl() const override { return GURL(kTestUploadUrl); }
  GURL GetInsecureUploadUrl() const override { return GURL(kTestUploadUrl); }
  std::string_view upload_mime_type() const override { return kTestMimeType; }
  MetricsLogUploader::MetricServiceType service_type() const override {
    return MetricsLogUploader::MetricServiceType::UMA;
  }

  TestLogStore log_store_;
};

class ReportingServiceTest : public testing::Test {
 public:
  ReportingServiceTest() {
    ReportingService::RegisterPrefs(testing_local_state_.registry());
  }

  ReportingServiceTest(const ReportingServiceTest&) = delete;
  ReportingServiceTest& operator=(const ReportingServiceTest&) = delete;

  ~ReportingServiceTest() override = default;

  PrefService* GetLocalState() { return &testing_local_state_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestMetricsServiceClient client_;

 private:
  TestingPrefServiceSimple testing_local_state_;
};

}  // namespace

TEST_F(ReportingServiceTest, BasicTest) {
  TestReportingService service(&client_, GetLocalState());
  service.AddLog(TestLog("log1"));
  service.AddLog(TestLog("log2"));

  service.EnableReporting();
  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(1, client_.uploader()->reporting_info().attempt_count());
  EXPECT_FALSE(client_.uploader()->reporting_info().has_last_response_code());

  client_.uploader()->CompleteUpload(404);
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetInitialBackoffInterval());
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(2, client_.uploader()->reporting_info().attempt_count());
  EXPECT_EQ(404, client_.uploader()->reporting_info().last_response_code());

  client_.uploader()->CompleteUpload(200);
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(1, client_.uploader()->reporting_info().attempt_count());
  EXPECT_EQ(200, client_.uploader()->reporting_info().last_response_code());

  client_.uploader()->CompleteUpload(200);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0U);
  EXPECT_FALSE(client_.uploader()->is_uploading());
}

TEST_F(ReportingServiceTest, UserIdLogsUploadedIfUserConsented) {
  uint64_t user_id = 12345;

  TestReportingService service(&client_, GetLocalState());
  service.AddLog(TestLog("log1", user_id));
  service.AddLog(TestLog("log2", user_id));
  service.EnableReporting();
  client_.AllowMetricUploadForUserId(user_id);

  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(1, client_.uploader()->reporting_info().attempt_count());
  EXPECT_FALSE(client_.uploader()->reporting_info().has_last_response_code());
  client_.uploader()->CompleteUpload(200);

  // Upload 2nd log and last response code logged.
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());
  EXPECT_EQ(200, client_.uploader()->reporting_info().last_response_code());
  EXPECT_TRUE(client_.uploader()->is_uploading());

  client_.uploader()->CompleteUpload(200);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0U);
  EXPECT_FALSE(client_.uploader()->is_uploading());
}

TEST_F(ReportingServiceTest, UserIdLogsNotUploadedIfUserNotConsented) {
  TestReportingService service(&client_, GetLocalState());
  service.AddLog(TestLog("log1", 12345));
  service.AddLog(TestLog("log2", 12345));
  service.EnableReporting();

  // Log with user id should never be in uploading state if user upload
  // disabled. |client_.uploader()| should be nullptr since it is lazily
  // created when a log is to be uploaded for the first time.
  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));
  EXPECT_EQ(client_.uploader(), nullptr);
}

TEST_F(ReportingServiceTest, ForceDiscard) {
  TestReportingService service(&client_, GetLocalState());
  service.AddLog(TestLog("log1"));

  service.EnableReporting();

  // Simulate the server returning a 500 error, which indicates that the server
  // is unhealthy.
  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));
  EXPECT_TRUE(client_.uploader()->is_uploading());
  client_.uploader()->CompleteUpload(500);
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetInitialBackoffInterval());
  // Verify that the log is not discarded so that it can be re-sent later.
  EXPECT_TRUE(service.HasUnsentLogs());
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Simulate the server returning a 500 error again, but this time, with
  // |force_discard| set to true.
  client_.uploader()->CompleteUpload(500, /*force_discard=*/true);
  // Verify that the log was discarded, and that |service| is not uploading
  // anymore since there are no more logs.
  EXPECT_FALSE(service.HasUnsentLogs());
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0U);
  EXPECT_FALSE(client_.uploader()->is_uploading());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ReportingServiceTest, ResetMetricsUploadBackoffOnForeground) {
  TestReportingService service(&client_, GetLocalState());
  service.AddLog(TestLog("log1"));
  service.AddLog(TestLog("log2"));
  service.AddLog(TestLog("log3"));

  service.EnableReporting();

  // Simulate the app being backgrounded.
  service.OnAppEnterBackground();

  // Simulate receiving a 105 (NAME_NOT_RESOLVED) error, which is one of the
  // many errors returned when trying to do a network request while in the
  // background on Android 15 and above.
  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));
  EXPECT_TRUE(client_.uploader()->is_uploading());
  client_.uploader()->CompleteUpload(105);
  // Verify that the upload has been re-scheduled with the the backoff interval.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1U);
  EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(),
            MetricsUploadScheduler::GetInitialBackoffInterval());
  // Simulate the app being foregrounded. The backoff should be reset, and the
  // upload should have be re-scheduled with the normal interval.
  service.OnAppEnterForeground();
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1U);
  EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(),
            MetricsUploadScheduler::GetUnsentLogsInterval());
  // Simulate the successful upload of log1.
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());
  EXPECT_TRUE(client_.uploader()->is_uploading());
  client_.uploader()->CompleteUpload(200);

  // The uploading of log2 should be scheduled with the normal interval as
  // usual.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1U);
  EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(),
            MetricsUploadScheduler::GetUnsentLogsInterval());
  // Simulate the app being backgrounded and the upload being initiated.
  service.OnAppEnterBackground();
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());
  EXPECT_TRUE(client_.uploader()->is_uploading());
  // Simulate the app being foregrounded while the upload is ongoing.
  service.OnAppEnterForeground();
  // Simulate the upload eventually completing with a 105 failure. The upload
  // should have be re-scheduled, but *not* with the backoff interval.
  client_.uploader()->CompleteUpload(105);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1U);
  EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(),
            MetricsUploadScheduler::GetUnsentLogsInterval());
  // Simulate the successful upload of log2.
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());
  EXPECT_TRUE(client_.uploader()->is_uploading());
  client_.uploader()->CompleteUpload(200);

  // The uploading of log3 should be scheduled with the normal interval as
  // usual.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1U);
  EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(),
            MetricsUploadScheduler::GetUnsentLogsInterval());
  // Simulate the upload failing (while still in the foreground).
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());
  EXPECT_TRUE(client_.uploader()->is_uploading());
  client_.uploader()->CompleteUpload(500);
  // The upload should be re-scheduled with the backoff interval.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1U);
  EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(),
            MetricsUploadScheduler::GetInitialBackoffInterval());
  // Simulate the app being backgrounded, and the re-scheduled upload failing
  // with yet another failure.
  service.OnAppEnterBackground();
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetInitialBackoffInterval());
  EXPECT_TRUE(client_.uploader()->is_uploading());
  client_.uploader()->CompleteUpload(105);
  // The upload should be re-scheduled with an even longer backoff interval.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1U);
  base::TimeDelta current_backoff_interval =
      task_environment_.NextMainThreadPendingTaskDelay();
  EXPECT_GT(current_backoff_interval,
            MetricsUploadScheduler::GetInitialBackoffInterval());
  // Simulate the app being foregrounded. The backoff interval should *not* have
  // been reset because the failures started while in the foreground.
  service.OnAppEnterForeground();
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1U);
  EXPECT_EQ(current_backoff_interval,
            task_environment_.NextMainThreadPendingTaskDelay());
  // Ditto when foregrounding during an upload.
  service.OnAppEnterBackground();
  task_environment_.FastForwardBy(current_backoff_interval);
  EXPECT_TRUE(client_.uploader()->is_uploading());
  service.OnAppEnterForeground();
  client_.uploader()->CompleteUpload(105);
  // The upload should have be re-scheduled, with an even longer backoff
  // interval (i.e. it should not be reset) since, again, failures started from
  // the foreground.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1U);
  base::TimeDelta current_backoff_interval2 =
      task_environment_.NextMainThreadPendingTaskDelay();
  EXPECT_GT(current_backoff_interval2, current_backoff_interval);
  // Finally, simulate the successful upload of log3. There should be no more
  // logs after that.
  task_environment_.FastForwardBy(current_backoff_interval2);
  EXPECT_TRUE(client_.uploader()->is_uploading());
  client_.uploader()->CompleteUpload(200);
  EXPECT_FALSE(service.HasUnsentLogs());
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0U);
  EXPECT_FALSE(client_.uploader()->is_uploading());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace metrics
