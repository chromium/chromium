// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_manager.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

class MetricsLogManagerTest : public testing::Test {
 public:
  MetricsLogManagerTest()
      : log_store_(&pref_service_,
                   client_.GetStorageLimits(),
                   /*signing_key=*/std::string(),
                   /*logs_event_manager=*/nullptr) {
    MetricsLogStore::RegisterPrefs(pref_service_.registry());
    log_store()->LoadPersistedUnsentLogs();
  }

  MetricsLogManagerTest(const MetricsLogManagerTest&) = delete;
  MetricsLogManagerTest& operator=(const MetricsLogManagerTest&) = delete;

  ~MetricsLogManagerTest() override {}

  MetricsLogStore* log_store() { return &log_store_; }

  MetricsLog* CreateLog(MetricsLog::LogType log_type) {
    return new MetricsLog("id", 0, log_type, &client_);
  }

  void SetClientVersion(const std::string& version) {
    client_.set_version_string(version);
  }

 private:
  TestMetricsServiceClient client_;
  TestingPrefServiceSimple pref_service_;
  MetricsLogStore log_store_;
};

}  // namespace

TEST_F(MetricsLogManagerTest, StandardFlow) {
  MetricsLogManager log_manager;

  // Make sure a new manager has a clean slate.
  EXPECT_EQ(nullptr, log_manager.current_log());

  // Check that the normal flow works.
  MetricsLog* initial_log = CreateLog(MetricsLog::INITIAL_STABILITY_LOG);
  log_manager.BeginLoggingWithLog(base::WrapUnique(initial_log));
  EXPECT_EQ(initial_log, log_manager.current_log());

  EXPECT_FALSE(log_store()->has_unsent_logs());
  log_manager.FinishCurrentLog(log_store());
  EXPECT_EQ(nullptr, log_manager.current_log());
  EXPECT_TRUE(log_store()->has_unsent_logs());

  MetricsLog* second_log = CreateLog(MetricsLog::ONGOING_LOG);
  log_manager.BeginLoggingWithLog(base::WrapUnique(second_log));
  EXPECT_EQ(second_log, log_manager.current_log());
}

// Make sure that interjecting logs updates the "current" log correctly.
TEST_F(MetricsLogManagerTest, InterjectedLog) {
  MetricsLogManager log_manager;

  MetricsLog* ongoing_log = CreateLog(MetricsLog::ONGOING_LOG);
  MetricsLog* temp_log = CreateLog(MetricsLog::INITIAL_STABILITY_LOG);

  log_manager.BeginLoggingWithLog(base::WrapUnique(ongoing_log));
  EXPECT_EQ(ongoing_log, log_manager.current_log());

  log_manager.PauseCurrentLog();
  EXPECT_EQ(nullptr, log_manager.current_log());

  log_manager.BeginLoggingWithLog(base::WrapUnique(temp_log));
  EXPECT_EQ(temp_log, log_manager.current_log());
  log_manager.FinishCurrentLog(log_store());
  EXPECT_EQ(nullptr, log_manager.current_log());

  log_manager.ResumePausedLog();
  EXPECT_EQ(ongoing_log, log_manager.current_log());
}

// Make sure that when one log is interjected by another, that finishing them
// creates logs of the correct type.
TEST_F(MetricsLogManagerTest, InterjectedLogPreservesType) {
  MetricsLogManager log_manager;

  log_manager.BeginLoggingWithLog(
      base::WrapUnique(CreateLog(MetricsLog::ONGOING_LOG)));
  log_manager.PauseCurrentLog();
  log_manager.BeginLoggingWithLog(
      base::WrapUnique(CreateLog(MetricsLog::INITIAL_STABILITY_LOG)));
  log_manager.FinishCurrentLog(log_store());
  log_manager.ResumePausedLog();
  // Finishing the interjecting inital log should have stored an initial log.
  EXPECT_EQ(1U, log_store()->initial_log_count());
  EXPECT_EQ(0U, log_store()->ongoing_log_count());

  // Finishing the interjected ongoing log should store an ongoing log.
  log_manager.FinishCurrentLog(log_store());
  EXPECT_EQ(1U, log_store()->initial_log_count());
  EXPECT_EQ(1U, log_store()->ongoing_log_count());
}

// Make sure that when a log is finished and the client's version is different
// from the version stored in the log, then the log_written_by_app_version field
// is gets set to the current version.
TEST_F(MetricsLogManagerTest, AppVersionChange) {
  MetricsLogManager log_manager;

  const std::string kNewVersion = "5.0.322.0-64-devel";
  SetClientVersion(kNewVersion);

  MetricsLog* metrics_log = CreateLog(MetricsLog::ONGOING_LOG);
  log_manager.BeginLoggingWithLog(base::WrapUnique(metrics_log));

  const std::string kOldVersion = "4.0.321.0-64-devel";
  metrics_log->UmaProtoForTest()->mutable_system_profile()->set_app_version(
      kOldVersion);

  log_manager.FinishCurrentLog(log_store());

  log_store()->StageNextLog();
  EXPECT_TRUE(log_store()->has_staged_log());
  ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(DecodeLogDataToProto(log_store()->staged_log(), &uma_log));

  EXPECT_EQ(kOldVersion, uma_log.system_profile().app_version());
  EXPECT_EQ(kNewVersion, uma_log.system_profile().log_written_by_app_version());
}

}  // namespace metrics
