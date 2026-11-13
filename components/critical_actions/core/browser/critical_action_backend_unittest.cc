// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_backend.h"

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/critical_actions/core/browser/critical_action_database.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "sql/database.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace critical_actions {

class CriticalActionBackendTest : public testing::Test {
 public:
  CriticalActionBackendTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestCriticalActions.db");
    backend_ = std::make_unique<CriticalActionBackend>(db_path_);
  }

  void TearDown() override { backend_.reset(); }

  void PrecreateDatabase() {
    CriticalActionDatabase setup_db(db_path_);
    ASSERT_EQ(setup_db.Init(), CriticalActionDatabase::InitStatus::kOk);
    setup_db.Close();
  }

  CriticalActionEntry CreateEntry(std::string id = "") {
    CriticalActionEntry entry;
    entry.critical_action_id =
        id.empty() ? base::Uuid::GenerateRandomV4().AsLowercaseString()
                   : std::move(id);
    entry.timestamp = base::Time::Now();
    entry.action_type = ActionType::kFormFill;
    return entry;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<CriticalActionBackend> backend_;
};

TEST_F(CriticalActionBackendTest, InitBackend) {
  backend_->Init();
  EXPECT_TRUE(base::PathExists(db_path_));
}

TEST_F(CriticalActionBackendTest, CallBeforeInitReturnsGracefully) {
  const std::string action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry;
  entry.critical_action_id = action_id;
  entry.timestamp = base::Time::Now();
  entry.action_type = ActionType::kFormFill;

  // Database is not initialized. All the operations should return gracefully
  // without crashing.
  backend_->AddCriticalAction(entry);
  backend_->SetCriticalActionsConversationId({action_id}, "conv_id");
  EXPECT_FALSE(backend_->GetCriticalAction(action_id).has_value());
  backend_->DeleteCriticalAction(action_id);
  backend_->DeleteCriticalActionsInTimeRange(base::Time::Now(),
                                             base::Time::Now());
  backend_->DeleteCriticalActionsByVisitIds({123, 456});
}

TEST_F(CriticalActionBackendTest, ForwardCallsToDatabase) {
  backend_->Init();

  const std::string action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry;
  entry.critical_action_id = action_id;
  entry.timestamp = base::Time::Now();
  entry.visit_id = base::RandIntInclusive(1, 1000000);
  entry.conversation_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.actor_task_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.action_type = ActionType::kFormFill;
  entry.metadata = "{}";

  // Verify basic crud operations are successfully forwarded.
  backend_->AddCriticalAction(entry);

  auto retrieved = backend_->GetCriticalAction(action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);

  backend_->DeleteCriticalAction(action_id);
  EXPECT_FALSE(backend_->GetCriticalAction(action_id).has_value());
}

TEST_F(CriticalActionBackendTest, SetCriticalActionsConversationId) {
  backend_->Init();

  const std::string action_id = "backend_action_1";
  const std::string task_id = "backend_task_1";
  const std::string conv_id = "backend_conv_1";

  CriticalActionEntry entry;
  entry.critical_action_id = action_id;
  entry.timestamp = base::Time::Now();
  entry.actor_task_id = task_id;
  entry.action_type = ActionType::kCredentialAccess;

  backend_->AddCriticalAction(entry);

  auto retrieved = backend_->GetCriticalAction(action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_TRUE(retrieved->conversation_id.empty());

  backend_->SetCriticalActionsConversationId({action_id}, conv_id);

  retrieved = backend_->GetCriticalAction(action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->conversation_id, conv_id);
}

TEST_F(CriticalActionBackendTest, BusyTimeoutRetriesAndRecoversOnTimer) {
  PrecreateDatabase();
  backend_->SetBusyTimeoutForTesting(base::TimeDelta());

  base::HistogramTester histogram_tester;
  sql::Database holder_db(sql::Database::Tag("CriticalActions"));
  ASSERT_TRUE(holder_db.Open(db_path_));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_BUSY);
    backend_->Init();
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 1);

  // First retry at +1s fails while `holder_db` still holds its SHARED lock.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_BUSY);
    task_environment_.FastForwardBy(base::Seconds(1));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 2);

  // Release the SHARED lock before the second retry (+2s) fires.
  holder_db.Close();
  task_environment_.FastForwardBy(base::Seconds(2));

  CriticalActionEntry entry = CreateEntry("recovered_after_timer");
  backend_->AddCriticalAction(entry);
  auto retrieved = backend_->GetCriticalAction(entry.critical_action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);

  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 2);
}

TEST_F(CriticalActionBackendTest,
       OperationTriggersImmediateRetryOnceLockReleasedBeforeTimerFires) {
  PrecreateDatabase();
  backend_->SetBusyTimeoutForTesting(base::TimeDelta());

  sql::Database holder_db(sql::Database::Tag("CriticalActions"));
  ASSERT_TRUE(holder_db.Open(db_path_));

  // Initial attempt + 2 timer retries (1s, 2s) fail; next timer is at +4s.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_BUSY);
    backend_->Init();
    task_environment_.FastForwardBy(base::Seconds(3));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  // Advance 1.5s into the 4s backoff window (past the 1s minimum interval) and
  // release `holder_db`. An incoming write must immediately recover and persist
  // without waiting for the remaining 2.5s of the timer.
  task_environment_.FastForwardBy(base::Milliseconds(1500));
  holder_db.Close();

  CriticalActionEntry entry = CreateEntry("recovered_on_demand");
  backend_->AddCriticalAction(entry);

  auto retrieved = backend_->GetCriticalAction(entry.critical_action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);
}

TEST_F(CriticalActionBackendTest,
       BurstDuringBackoffWindowDoesNotExhaustRetriesEarly) {
  PrecreateDatabase();
  backend_->SetBusyTimeoutForTesting(base::TimeDelta());

  base::HistogramTester histogram_tester;
  sql::Database holder_db(sql::Database::Tag("CriticalActions"));
  ASSERT_TRUE(holder_db.Open(db_path_));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_BUSY);
    backend_->Init();

    // A burst of calls within <1s of the failed attempt must not trigger extra
    // Init() attempts.
    for (int i = 0; i < 5; ++i) {
      backend_->AddCriticalAction(CreateEntry());
    }
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 1);

  holder_db.Close();
  task_environment_.FastForwardBy(base::Seconds(1));

  CriticalActionEntry entry = CreateEntry("after_burst");
  backend_->AddCriticalAction(entry);
  EXPECT_TRUE(
      backend_->GetCriticalAction(entry.critical_action_id).has_value());
}

TEST_F(CriticalActionBackendTest, ExhaustedBusyRetriesStopsRetrying) {
  PrecreateDatabase();
  backend_->SetBusyTimeoutForTesting(base::TimeDelta());

  base::HistogramTester histogram_tester;
  sql::Database holder_db(sql::Database::Tag("CriticalActions"));
  ASSERT_TRUE(holder_db.Open(db_path_));

  // Initial attempt + 5 exponential backoff retries (1 + 2 + 4 + 8 + 16 = 31s).
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_BUSY);
    backend_->Init();
    task_environment_.FastForwardBy(base::Seconds(31));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 6);

  // Once retries are exhausted, no further retries run even after lock release.
  holder_db.Close();
  task_environment_.FastForwardBy(base::Seconds(60));
  CriticalActionEntry entry = CreateEntry("after_exhausted");
  backend_->AddCriticalAction(entry);
  EXPECT_FALSE(
      backend_->GetCriticalAction(entry.critical_action_id).has_value());
  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 6);
}

TEST_F(CriticalActionBackendTest, NonBusyInitErrorDoesNotScheduleRetries) {
  base::HistogramTester histogram_tester;
  CriticalActionBackend dir_backend(temp_dir_.GetPath());
  dir_backend.Init();

  histogram_tester.ExpectTotalCount(
      "Sql.Database.Open.FirstAttempt.Error.CriticalActions", 1);

  task_environment_.FastForwardBy(base::Seconds(60));
  dir_backend.AddCriticalAction(CreateEntry("unopenable"));

  histogram_tester.ExpectTotalCount(
      "Sql.Database.Open.FirstAttempt.Error.CriticalActions", 1);
}

TEST_F(CriticalActionBackendTest, DestroyWhileRetryPendingCancelsTimerCleanly) {
  PrecreateDatabase();
  backend_->SetBusyTimeoutForTesting(base::TimeDelta());

  base::HistogramTester histogram_tester;
  sql::Database holder_db(sql::Database::Tag("CriticalActions"));
  ASSERT_TRUE(holder_db.Open(db_path_));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_BUSY);
    backend_->Init();
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 1);

  backend_.reset();
  holder_db.Close();
  task_environment_.FastForwardBy(base::Seconds(60));

  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 1);
}

}  // namespace critical_actions
