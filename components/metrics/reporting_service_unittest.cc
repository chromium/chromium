// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/reporting_service.h"

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/hash/sha1.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/log_store.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

namespace {

const char kTestUploadUrl[] = "test_url";
const char kTestMimeType[] = "test_mime_type";

class TestLogStore : public LogStore {
 public:
  TestLogStore() {}
  ~TestLogStore() {}

  void AddLog(const std::string& log) { logs_.push_back(log); }

  // LogStore:
  bool has_unsent_logs() const override { return !logs_.empty(); }
  bool has_staged_log() const override { return !staged_log_hash_.empty(); }
  const std::string& staged_log() const override { return logs_.front(); }
  const std::string& staged_log_hash() const override {
    return staged_log_hash_;
  }
  const std::string& staged_log_signature() const override {
    return base::EmptyString();
  }
  void StageNextLog() override {
    if (has_unsent_logs())
      staged_log_hash_ = base::SHA1HashString(logs_.front());
  }
  void DiscardStagedLog() override {
    if (!has_staged_log())
      return;
    logs_.pop_front();
    staged_log_hash_.clear();
  }
  void PersistUnsentLogs() const override {}
  void LoadPersistedUnsentLogs() override {}

 private:
  std::string staged_log_hash_;
  std::deque<std::string> logs_;
};

class TestReportingService : public ReportingService {
 public:
  TestReportingService(MetricsServiceClient* client, PrefService* local_state)
      : ReportingService(client, local_state, 100) {
    Initialize();
  }
  ~TestReportingService() override {}

  void AddLog(const std::string& log) { log_store_.AddLog(log); }

 private:
  // ReportingService:
  LogStore* log_store() override { return &log_store_; }
  GURL GetUploadUrl() const override { return GURL(kTestUploadUrl); }
  GURL GetInsecureUploadUrl() const override { return GURL(kTestUploadUrl); }
  base::StringPiece upload_mime_type() const override { return kTestMimeType; }
  MetricsLogUploader::MetricServiceType service_type() const override {
    return MetricsLogUploader::MetricServiceType::UMA;
  }

  TestLogStore log_store_;

  DISALLOW_COPY_AND_ASSIGN(TestReportingService);
};

class ReportingServiceTest : public testing::Test {
 public:
  ReportingServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_) {
    ReportingService::RegisterPrefs(testing_local_state_.registry());
  }

  ~ReportingServiceTest() override {}

  PrefService* GetLocalState() { return &testing_local_state_; }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  TestMetricsServiceClient client_;

 private:
  TestingPrefServiceSimple testing_local_state_;

  DISALLOW_COPY_AND_ASSIGN(ReportingServiceTest);
};

}  // namespace

TEST_F(ReportingServiceTest, BasicTest) {
  TestReportingService service(&client_, GetLocalState());
  service.AddLog("log1");
  service.AddLog("log2");

  service.EnableReporting();
  task_runner_->RunPendingTasks();
  client_.uploader()->is_uploading();
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(1, client_.uploader()->reporting_info().attempt_count());
  EXPECT_FALSE(client_.uploader()->reporting_info().has_last_response_code());

  client_.uploader()->CompleteUpload(404);
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(2, client_.uploader()->reporting_info().attempt_count());
  EXPECT_EQ(404, client_.uploader()->reporting_info().last_response_code());

  client_.uploader()->CompleteUpload(200);
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client_.uploader()->is_uploading());
  EXPECT_EQ(1, client_.uploader()->reporting_info().attempt_count());
  EXPECT_EQ(200, client_.uploader()->reporting_info().last_response_code());

  client_.uploader()->CompleteUpload(200);
  EXPECT_EQ(0U, task_runner_->NumPendingTasks());
  EXPECT_FALSE(client_.uploader()->is_uploading());
}

}  // namespace metrics
