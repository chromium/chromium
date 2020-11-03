// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_store.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

class MetricsLogStoreTest : public testing::Test {
 public:
  MetricsLogStoreTest() {
    MetricsLogStore::RegisterPrefs(pref_service_.registry());
  }
  ~MetricsLogStoreTest() override {}

  MetricsLog* CreateLog(MetricsLog::LogType log_type) {
    return new MetricsLog("id", 0, log_type, &client_);
  }

  // Returns the stored number of logs of the given type.
  size_t TypeCount(MetricsLog::LogType log_type) {
    const char* pref = log_type == MetricsLog::INITIAL_STABILITY_LOG
                           ? prefs::kMetricsInitialLogs
                           : prefs::kMetricsOngoingLogs;
    return pref_service_.GetList(pref)->GetSize();
  }

  TestMetricsServiceClient client_;
  TestingPrefServiceSimple pref_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsLogStoreTest);
};

}  // namespace

TEST_F(MetricsLogStoreTest, StandardFlow) {
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            std::string());
  log_store.LoadPersistedUnsentLogs();

  // Make sure a new manager has a clean slate.
  EXPECT_FALSE(log_store.has_staged_log());
  EXPECT_FALSE(log_store.has_unsent_logs());

  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, base::nullopt);
  EXPECT_TRUE(log_store.has_unsent_logs());
  EXPECT_FALSE(log_store.has_staged_log());

  log_store.StageNextLog();
  EXPECT_TRUE(log_store.has_staged_log());
  EXPECT_FALSE(log_store.staged_log().empty());

  log_store.DiscardStagedLog();
  EXPECT_FALSE(log_store.has_staged_log());
  EXPECT_FALSE(log_store.has_unsent_logs());
}

TEST_F(MetricsLogStoreTest, StoreAndLoad) {
  // Set up some in-progress logging in a scoped log manager simulating the
  // leadup to quitting, then persist as would be done on quit.
  {
    MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                              std::string());
    log_store.LoadPersistedUnsentLogs();
    EXPECT_FALSE(log_store.has_unsent_logs());
    log_store.StoreLog("a", MetricsLog::ONGOING_LOG, base::nullopt);
    log_store.TrimAndPersistUnsentLogs();
    EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, TypeCount(MetricsLog::ONGOING_LOG));
  }

  // Relaunch load and store more logs.
  {
    MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                              std::string());
    log_store.LoadPersistedUnsentLogs();
    EXPECT_TRUE(log_store.has_unsent_logs());
    EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, TypeCount(MetricsLog::ONGOING_LOG));
    log_store.StoreLog("x", MetricsLog::INITIAL_STABILITY_LOG, base::nullopt);
    log_store.StageNextLog();
    log_store.StoreLog("b", MetricsLog::ONGOING_LOG, base::nullopt);

    EXPECT_TRUE(log_store.has_unsent_logs());
    EXPECT_TRUE(log_store.has_staged_log());
    EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, TypeCount(MetricsLog::ONGOING_LOG));

    log_store.TrimAndPersistUnsentLogs();
    EXPECT_EQ(1U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(2U, TypeCount(MetricsLog::ONGOING_LOG));
  }

  // Relaunch and verify that once logs are handled they are not re-persisted.
  {
    MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                              std::string());
    log_store.LoadPersistedUnsentLogs();
    EXPECT_TRUE(log_store.has_unsent_logs());

    log_store.StageNextLog();
    log_store.DiscardStagedLog();
    // The initial log should be sent first; update the persisted storage to
    // verify.
    log_store.TrimAndPersistUnsentLogs();
    EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(2U, TypeCount(MetricsLog::ONGOING_LOG));

    // Handle the first ongoing log.
    log_store.StageNextLog();
    log_store.DiscardStagedLog();
    EXPECT_TRUE(log_store.has_unsent_logs());

    // Handle the last log.
    log_store.StageNextLog();
    log_store.DiscardStagedLog();
    EXPECT_FALSE(log_store.has_unsent_logs());

    // Nothing should have changed "on disk" since TrimAndPersistUnsentLogs
    // hasn't been called again.
    EXPECT_EQ(2U, TypeCount(MetricsLog::ONGOING_LOG));
    // Persist, and make sure nothing is left.
    log_store.TrimAndPersistUnsentLogs();
    EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(0U, TypeCount(MetricsLog::ONGOING_LOG));
  }
}

TEST_F(MetricsLogStoreTest, StoreStagedOngoingLog) {
  // Ensure that types are preserved when storing staged logs.
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            std::string());
  log_store.LoadPersistedUnsentLogs();
  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, base::nullopt);
  log_store.StageNextLog();
  log_store.TrimAndPersistUnsentLogs();

  EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
  EXPECT_EQ(1U, TypeCount(MetricsLog::ONGOING_LOG));
}

TEST_F(MetricsLogStoreTest, StoreStagedInitialLog) {
  // Ensure that types are preserved when storing staged logs.
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            std::string());
  log_store.LoadPersistedUnsentLogs();
  log_store.StoreLog("b", MetricsLog::INITIAL_STABILITY_LOG, base::nullopt);
  log_store.StageNextLog();
  log_store.TrimAndPersistUnsentLogs();

  EXPECT_EQ(1U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
  EXPECT_EQ(0U, TypeCount(MetricsLog::ONGOING_LOG));
}

TEST_F(MetricsLogStoreTest, LargeLogDiscarding) {
  // Set the size threshold very low, to verify that it's honored.
  client_.set_max_ongoing_log_size(1);
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            std::string());
  log_store.LoadPersistedUnsentLogs();

  log_store.StoreLog("persisted", MetricsLog::INITIAL_STABILITY_LOG,
                     base::nullopt);
  log_store.StoreLog("not_persisted", MetricsLog::ONGOING_LOG, base::nullopt);

  // Only the stability log should be written out, due to the threshold.
  log_store.TrimAndPersistUnsentLogs();
  EXPECT_EQ(1U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
  EXPECT_EQ(0U, TypeCount(MetricsLog::ONGOING_LOG));
}

TEST_F(MetricsLogStoreTest, DiscardOrder) {
  // Ensure that the correct log is discarded if new logs are pushed while
  // a log is staged.
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            std::string());
  log_store.LoadPersistedUnsentLogs();

  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, base::nullopt);
  log_store.StoreLog("b", MetricsLog::ONGOING_LOG, base::nullopt);
  log_store.StageNextLog();
  log_store.StoreLog("c", MetricsLog::INITIAL_STABILITY_LOG, base::nullopt);
  EXPECT_EQ(2U, log_store.ongoing_log_count());
  EXPECT_EQ(1U, log_store.initial_log_count());
  // Should discard the ongoing log staged earlier.
  log_store.DiscardStagedLog();
  EXPECT_EQ(1U, log_store.ongoing_log_count());
  EXPECT_EQ(1U, log_store.initial_log_count());
  // Initial log should be staged next.
  log_store.StageNextLog();
  log_store.DiscardStagedLog();
  EXPECT_EQ(1U, log_store.ongoing_log_count());
  EXPECT_EQ(0U, log_store.initial_log_count());
}

}  // namespace metrics
