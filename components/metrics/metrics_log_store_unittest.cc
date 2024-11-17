// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_store.h"

#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

const char kTestPrefName[] = "TestPref";

class TestUnsentLogStore : public UnsentLogStore {
 public:
  explicit TestUnsentLogStore(PrefService* service)
      : UnsentLogStore(std::make_unique<UnsentLogStoreMetricsImpl>(),
                       service,
                       kTestPrefName,
                       nullptr,
                       // Set to 3 so logs are not dropped in the test.
                       UnsentLogStore::UnsentLogStoreLimits{
                           .min_log_count = 3,
                       },
                       /*signing_key=*/std::string(),
                       /*logs_event_manager=*/nullptr) {}
  ~TestUnsentLogStore() override = default;

  TestUnsentLogStore(const TestUnsentLogStore&) = delete;
  TestUnsentLogStore& operator=(const TestUnsentLogStore&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry) {
    registry->RegisterListPref(kTestPrefName);
  }
};

class MetricsLogStoreTest : public testing::Test {
 public:
  MetricsLogStoreTest() {
    MetricsLogStore::RegisterPrefs(pref_service_.registry());
    TestUnsentLogStore::RegisterPrefs(pref_service_.registry());
  }

  MetricsLogStoreTest(const MetricsLogStoreTest&) = delete;
  MetricsLogStoreTest& operator=(const MetricsLogStoreTest&) = delete;

  ~MetricsLogStoreTest() override = default;

  MetricsLog* CreateLog(MetricsLog::LogType log_type) {
    return new MetricsLog("0a94430b-18e5-43c8-a657-580f7e855ce1", 0, log_type,
                          &client_);
  }

  // Returns the stored number of logs of the given type.
  size_t TypeCount(MetricsLog::LogType log_type) {
    const char* pref = log_type == MetricsLog::INITIAL_STABILITY_LOG
                           ? prefs::kMetricsInitialLogs
                           : prefs::kMetricsOngoingLogs;
    return pref_service_.GetList(pref).size();
  }

  TestMetricsServiceClient client_;
  TestingPrefServiceSimple pref_service_;
};

}  // namespace

TEST_F(MetricsLogStoreTest, StandardFlow) {
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  log_store.LoadPersistedUnsentLogs();

  // Make sure a new manager has a clean slate.
  EXPECT_FALSE(log_store.has_staged_log());
  EXPECT_FALSE(log_store.has_unsent_logs());

  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
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
                              /*signing_key=*/std::string(),
                              /*logs_event_manager=*/nullptr);
    log_store.LoadPersistedUnsentLogs();
    EXPECT_FALSE(log_store.has_unsent_logs());
    log_store.StoreLog("a", MetricsLog::ONGOING_LOG, LogMetadata(),
                       MetricsLogsEventManager::CreateReason::kUnknown);
    log_store.TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);
    EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, TypeCount(MetricsLog::ONGOING_LOG));
  }

  // Relaunch load and store more logs.
  {
    MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                              /*signing_key=*/std::string(),
                              /*logs_event_manager=*/nullptr);
    log_store.LoadPersistedUnsentLogs();
    EXPECT_TRUE(log_store.has_unsent_logs());
    EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, TypeCount(MetricsLog::ONGOING_LOG));
    log_store.StoreLog("x", MetricsLog::INITIAL_STABILITY_LOG, LogMetadata(),
                       MetricsLogsEventManager::CreateReason::kUnknown);
    log_store.StageNextLog();
    log_store.StoreLog("b", MetricsLog::ONGOING_LOG, LogMetadata(),
                       MetricsLogsEventManager::CreateReason::kUnknown);

    EXPECT_TRUE(log_store.has_unsent_logs());
    EXPECT_TRUE(log_store.has_staged_log());
    EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, TypeCount(MetricsLog::ONGOING_LOG));

    log_store.TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);
    EXPECT_EQ(1U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(2U, TypeCount(MetricsLog::ONGOING_LOG));
  }

  // Relaunch and verify that once logs are handled they are not re-persisted.
  {
    MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                              /*signing_key=*/std::string(),
                              /*logs_event_manager=*/nullptr);
    log_store.LoadPersistedUnsentLogs();
    EXPECT_TRUE(log_store.has_unsent_logs());

    log_store.StageNextLog();
    log_store.DiscardStagedLog();
    // The initial log should be sent first; update the persisted storage to
    // verify.
    log_store.TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);
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
    log_store.TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);
    EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
    EXPECT_EQ(0U, TypeCount(MetricsLog::ONGOING_LOG));
  }
}

TEST_F(MetricsLogStoreTest, StoreStagedOngoingLog) {
  // Ensure that types are preserved when storing staged logs.
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  log_store.LoadPersistedUnsentLogs();
  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StageNextLog();
  log_store.TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);

  EXPECT_EQ(0U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
  EXPECT_EQ(1U, TypeCount(MetricsLog::ONGOING_LOG));
}

TEST_F(MetricsLogStoreTest, StoreStagedInitialLog) {
  // Ensure that types are preserved when storing staged logs.
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  log_store.LoadPersistedUnsentLogs();
  log_store.StoreLog("b", MetricsLog::INITIAL_STABILITY_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StageNextLog();
  log_store.TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);

  EXPECT_EQ(1U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
  EXPECT_EQ(0U, TypeCount(MetricsLog::ONGOING_LOG));
}

TEST_F(MetricsLogStoreTest, LargeLogDiscarding) {
  // Set the size threshold very low, to verify that it's honored.
  client_.set_max_ongoing_log_size_bytes(1);
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  log_store.LoadPersistedUnsentLogs();

  log_store.StoreLog("persisted", MetricsLog::INITIAL_STABILITY_LOG,
                     LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StoreLog("not_persisted", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);

  // Only the stability log should be written out, due to the threshold.
  log_store.TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);
  EXPECT_EQ(1U, TypeCount(MetricsLog::INITIAL_STABILITY_LOG));
  EXPECT_EQ(0U, TypeCount(MetricsLog::ONGOING_LOG));
}

TEST_F(MetricsLogStoreTest, DiscardOrder) {
  // Ensure that the correct log is discarded if new logs are pushed while
  // a log is staged.
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  log_store.LoadPersistedUnsentLogs();

  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StoreLog("b", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StageNextLog();
  log_store.StoreLog("c", MetricsLog::INITIAL_STABILITY_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
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

TEST_F(MetricsLogStoreTest, WritesToAlternateOngoingLogStore) {
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  std::unique_ptr<TestUnsentLogStore> alternate_ongoing_log_store =
      std::make_unique<TestUnsentLogStore>(&pref_service_);
  TestUnsentLogStore* alternate_ongoing_log_store_ptr =
      alternate_ongoing_log_store.get();

  // Needs to be called before writing logs to alternate ongoing store since
  // SetAlternateOngoingLogStore loads persisted unsent logs and assumes that
  // the native initial and ongoing unsent logs have already been loaded.
  log_store.LoadPersistedUnsentLogs();

  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.SetAlternateOngoingLogStore(std::move(alternate_ongoing_log_store));
  log_store.StoreLog("b", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StoreLog("c", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);

  EXPECT_EQ(1U, log_store.ongoing_log_count());
  EXPECT_EQ(2U, alternate_ongoing_log_store_ptr->size());
}

TEST_F(MetricsLogStoreTest, AlternateOngoingLogStoreGetsEventsLogsManager) {
  // Create a MetricsLogStore with a MetricsLogsEventManager.
  MetricsLogsEventManager logs_event_manager;
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(), &logs_event_manager);

  // Create an UnsentLogStore that will be used as an alternate ongoing log
  // store.
  std::unique_ptr<TestUnsentLogStore> alternate_ongoing_log_store =
      std::make_unique<TestUnsentLogStore>(&pref_service_);
  TestUnsentLogStore* alternate_ongoing_log_store_ptr =
      alternate_ongoing_log_store.get();

  // Verify that |alternate_ongoing_log_store| has no logs event manager.
  EXPECT_FALSE(
      alternate_ongoing_log_store_ptr->GetLogsEventManagerForTesting());

  // Needs to be called before we can set |log_store|'s alternate ongoing log
  // store.
  log_store.LoadPersistedUnsentLogs();

  // Verify that after setting |log_store|'s alternate ongoing log store to
  // |alternate_ongoing_log_store|, the latter should have the former's logs
  // event manager.
  log_store.SetAlternateOngoingLogStore(std::move(alternate_ongoing_log_store));
  EXPECT_EQ(alternate_ongoing_log_store_ptr->GetLogsEventManagerForTesting(),
            &logs_event_manager);
}

TEST_F(MetricsLogStoreTest, StagesInitialOverBothOngoing) {
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  std::unique_ptr<TestUnsentLogStore> alternate_ongoing_log_store =
      std::make_unique<TestUnsentLogStore>(&pref_service_);
  TestUnsentLogStore* alternate_ongoing_log_store_ptr =
      alternate_ongoing_log_store.get();

  // Needs to be called before writing logs to alternate ongoing store since
  // SetAlternateOngoingLogStore loads persisted unsent logs and assumes that
  // the native initial and ongoing unsent logs have already been loaded.
  log_store.LoadPersistedUnsentLogs();

  log_store.StoreLog("a", MetricsLog::INITIAL_STABILITY_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StoreLog("b", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.SetAlternateOngoingLogStore(std::move(alternate_ongoing_log_store));
  log_store.StoreLog("c", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StageNextLog();
  log_store.DiscardStagedLog();

  // Discarded log should be from initial_log_store.
  EXPECT_EQ(0U, log_store.initial_log_count());
  EXPECT_EQ(1U, log_store.ongoing_log_count());
  EXPECT_EQ(1U, alternate_ongoing_log_store_ptr->size());
}

TEST_F(MetricsLogStoreTest, StagesAlternateOverOngoing) {
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  std::unique_ptr<TestUnsentLogStore> alternate_ongoing_log_store =
      std::make_unique<TestUnsentLogStore>(&pref_service_);
  TestUnsentLogStore* alternate_ongoing_log_store_ptr =
      alternate_ongoing_log_store.get();

  // Needs to be called before writing logs to alternate ongoing store since
  // SetAlternateOngoingLogStore loads persisted unsent logs and assumes that
  // the native initial and ongoing unsent logs have already been loaded.
  log_store.LoadPersistedUnsentLogs();

  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.SetAlternateOngoingLogStore(std::move(alternate_ongoing_log_store));
  log_store.StoreLog("b", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StageNextLog();
  log_store.DiscardStagedLog();

  // Discarded log should be from alternate_ongoing_log_store.
  EXPECT_EQ(1U, log_store.ongoing_log_count());
  EXPECT_EQ(0U, alternate_ongoing_log_store_ptr->size());
}

TEST_F(MetricsLogStoreTest,
       UnboundAlternateOngoingLogStoreWritesToNativeOngoing) {
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  std::unique_ptr<TestUnsentLogStore> alternate_ongoing_log_store =
      std::make_unique<TestUnsentLogStore>(&pref_service_);

  // Needs to be called before writing logs to alternate ongoing store since
  // SetAlternateOngoingLogStore loads persisted unsent logs and assumes that
  // the native initial and ongoing unsent logs have already been loaded.
  log_store.LoadPersistedUnsentLogs();

  log_store.SetAlternateOngoingLogStore(std::move(alternate_ongoing_log_store));
  // Should be written to alternate ongoing log store.
  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);

  log_store.UnsetAlternateOngoingLogStore();

  // Should be in native ongoing log store.
  log_store.StoreLog("b", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);
  log_store.StoreLog("c", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);

  EXPECT_EQ(2U, log_store.ongoing_log_count());
}

TEST_F(MetricsLogStoreTest,
       StageOngoingLogWhenAlternateOngoingLogStoreIsEmpty) {
  MetricsLogStore log_store(&pref_service_, client_.GetStorageLimits(),
                            /*signing_key=*/std::string(),
                            /*logs_event_manager=*/nullptr);
  std::unique_ptr<TestUnsentLogStore> alternate_ongoing_log_store =
      std::make_unique<TestUnsentLogStore>(&pref_service_);

  // Needs to be called before writing logs to alternate ongoing store since
  // SetAlternateOngoingLogStore loads persisted unsent logs and assumes that
  // the native initial and ongoing unsent logs have already been loaded.
  log_store.LoadPersistedUnsentLogs();

  // Should be written to ongoing log store.
  log_store.StoreLog("a", MetricsLog::ONGOING_LOG, LogMetadata(),
                     MetricsLogsEventManager::CreateReason::kUnknown);

  // Ensure that the log was stored in ongoing log.
  EXPECT_EQ(1U, log_store.ongoing_log_count());

  log_store.SetAlternateOngoingLogStore(std::move(alternate_ongoing_log_store));

  log_store.StageNextLog();
  log_store.DiscardStagedLog();

  // Discarded log should be from ongoing.
  EXPECT_EQ(0U, log_store.ongoing_log_count());
}

}  // namespace metrics
