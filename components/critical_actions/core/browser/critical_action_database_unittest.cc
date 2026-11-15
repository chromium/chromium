// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_database.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace critical_actions {

using ::testing::HasSubstr;
using ::testing::Not;
using InitStatus = CriticalActionDatabase::InitStatus;

class CriticalActionDatabaseTest : public testing::Test {
 public:
  CriticalActionDatabaseTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestCriticalActions.db");
  }

  // Returns an entry with valid default values and a unique random ID.
  // Tests only need to override fields relevant to their specific scenario.
  CriticalActionEntry CreateDefaultEntry() {
    CriticalActionEntry entry;
    entry.critical_action_id =
        base::Uuid::GenerateRandomV4().AsLowercaseString();
    entry.timestamp = base::Time::Now();
    entry.action_type = ActionType::kFormFill;
    return entry;
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

TEST_F(CriticalActionDatabaseTest, InitDatabase) {
  CriticalActionDatabase database(db_path_);
  EXPECT_EQ(database.Init(), InitStatus::kOk);
  EXPECT_TRUE(base::PathExists(db_path_));
  database.Close();
}

TEST_F(CriticalActionDatabaseTest, AddAndGetEntry) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  CriticalActionEntry entry = CreateDefaultEntry();
  entry.visit_id = base::RandIntInclusive(1, 1000000);
  entry.conversation_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.conversation_anchor_id = "turn_1";
  entry.actor_task_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.metadata = "{\"key\": \"val\"}";

  EXPECT_TRUE(database.AddCriticalAction(entry));

  auto retrieved = database.GetCriticalAction(entry.critical_action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);
  EXPECT_EQ(retrieved->critical_action_id, entry.critical_action_id);
  EXPECT_EQ(retrieved->actor_task_id, entry.actor_task_id);
  EXPECT_EQ(retrieved->conversation_id, entry.conversation_id);
  EXPECT_EQ(retrieved->conversation_anchor_id, entry.conversation_anchor_id);
  EXPECT_EQ(retrieved->metadata, entry.metadata);

  database.Close();
}

// Rows with an action_type unknown to this version (e.g. written by a newer
// version, or corrupted) are read as ActionType::kUnknown.
TEST_F(CriticalActionDatabaseTest, UnknownActionTypeReadsAsUnknown) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  CriticalActionEntry newer_entry = CreateDefaultEntry();
  newer_entry.action_type =
      static_cast<ActionType>(static_cast<int>(ActionType::kMaxValue) + 1);
  ASSERT_TRUE(database.AddCriticalAction(newer_entry));

  CriticalActionEntry negative_entry = CreateDefaultEntry();
  negative_entry.action_type = static_cast<ActionType>(-1);
  ASSERT_TRUE(database.AddCriticalAction(negative_entry));

  auto retrieved_newer =
      database.GetCriticalAction(newer_entry.critical_action_id);
  ASSERT_TRUE(retrieved_newer.has_value());
  EXPECT_EQ(retrieved_newer->action_type, ActionType::kUnknown);

  auto retrieved_negative =
      database.GetCriticalAction(negative_entry.critical_action_id);
  ASSERT_TRUE(retrieved_negative.has_value());
  EXPECT_EQ(retrieved_negative->action_type, ActionType::kUnknown);

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, AddDuplicateEntryFails) {
  base::HistogramTester histogram_tester;
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  CriticalActionEntry entry = CreateDefaultEntry();
  entry.action_type = ActionType::kDownload;

  EXPECT_TRUE(database.AddCriticalAction(entry));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CONSTRAINT_PRIMARYKEY);

  // Primary key constraint should make duplicate insertion fail (return false).
  EXPECT_FALSE(database.AddCriticalAction(entry));
  EXPECT_TRUE(expecter.SawExpectedErrors());

  histogram_tester.ExpectBucketCount(
      "CriticalActions.Database.SqliteError",
      sql::SqliteLoggedResultCode::kConstraintPrimaryKey, 1);

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, GetNonExistentReturnsNullopt) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  auto retrieved = database.GetCriticalAction(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  EXPECT_FALSE(retrieved.has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, DeleteSingleEntry) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  CriticalActionEntry entry = CreateDefaultEntry();
  entry.action_type = ActionType::kSettingChange;

  EXPECT_TRUE(database.AddCriticalAction(entry));
  EXPECT_TRUE(database.DeleteCriticalAction(entry.critical_action_id));

  auto retrieved = database.GetCriticalAction(entry.critical_action_id);
  EXPECT_FALSE(retrieved.has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, DeleteInTimeRange) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  base::Time base_time = base::Time::Now();

  CriticalActionEntry entry1 = CreateDefaultEntry();
  entry1.timestamp = base_time - base::Hours(2);
  ASSERT_TRUE(database.AddCriticalAction(entry1));

  CriticalActionEntry entry2 = CreateDefaultEntry();
  entry2.timestamp = base_time;
  ASSERT_TRUE(database.AddCriticalAction(entry2));

  CriticalActionEntry entry3 = CreateDefaultEntry();
  entry3.timestamp = base_time + base::Hours(2);
  ASSERT_TRUE(database.AddCriticalAction(entry3));

  // Delete everything around the middle entry (base_time).
  // Time range is inclusive of start, exclusive of end.
  // Start from -1 hour to +1 hour.
  EXPECT_TRUE(database.DeleteCriticalActionsInTimeRange(
      base_time - base::Hours(1), base_time + base::Hours(1)));

  // entry1 should remain (2 hours ago)
  EXPECT_TRUE(
      database.GetCriticalAction(entry1.critical_action_id).has_value());
  // entry2 should have been deleted (exactly base_time)
  EXPECT_FALSE(
      database.GetCriticalAction(entry2.critical_action_id).has_value());
  // entry3 should remain (2 hours from now)
  EXPECT_TRUE(
      database.GetCriticalAction(entry3.critical_action_id).has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, DeleteByVisitIds) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  int64_t visit_id_1 = base::RandIntInclusive(1, 1000000);
  int64_t visit_id_2 = visit_id_1 + 1;
  int64_t visit_id_3 = visit_id_1 + 2;

  CriticalActionEntry entry1 = CreateDefaultEntry();
  entry1.visit_id = visit_id_1;
  ASSERT_TRUE(database.AddCriticalAction(entry1));

  CriticalActionEntry entry2 = CreateDefaultEntry();
  entry2.visit_id = visit_id_2;
  entry2.action_type = ActionType::kDownload;
  ASSERT_TRUE(database.AddCriticalAction(entry2));

  CriticalActionEntry entry3 = CreateDefaultEntry();
  entry3.visit_id = visit_id_3;
  entry3.action_type = ActionType::kSettingChange;
  ASSERT_TRUE(database.AddCriticalAction(entry3));

  // Deleting visit_id_1 and visit_id_3.
  EXPECT_TRUE(
      database.DeleteCriticalActionsByVisitIds({visit_id_1, visit_id_3}));

  // entry1 and entry3 should be deleted, entry2 should remain.
  EXPECT_FALSE(
      database.GetCriticalAction(entry1.critical_action_id).has_value());
  EXPECT_TRUE(
      database.GetCriticalAction(entry2.critical_action_id).has_value());
  EXPECT_FALSE(
      database.GetCriticalAction(entry3.critical_action_id).has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, GetCriticalActionsWithOptions) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  base::Time base_time = base::Time::Now();

  const std::string conv_id_1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string conv_id_2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string task_id_1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string task_id_2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  CriticalActionEntry entry1 = CreateDefaultEntry();
  entry1.timestamp = base_time - base::Hours(3);
  entry1.action_type = ActionType::kFormFill;
  entry1.conversation_id = conv_id_1;
  entry1.actor_task_id = task_id_1;
  entry1.visit_id = 101;
  ASSERT_TRUE(database.AddCriticalAction(entry1));

  CriticalActionEntry entry2 = CreateDefaultEntry();
  entry2.timestamp = base_time - base::Hours(2);
  entry2.action_type = ActionType::kDownload;
  entry2.conversation_id = conv_id_2;
  entry2.actor_task_id = task_id_1;
  entry2.visit_id = 102;
  ASSERT_TRUE(database.AddCriticalAction(entry2));

  CriticalActionEntry entry3 = CreateDefaultEntry();
  entry3.timestamp = base_time - base::Hours(1);
  entry3.action_type = ActionType::kSettingChange;
  entry3.conversation_id = conv_id_1;
  entry3.actor_task_id = task_id_2;
  entry3.visit_id = 103;
  ASSERT_TRUE(database.AddCriticalAction(entry3));

  // Test 1: Query all, verify order (timestamp DESC: entry3 -> entry2 ->
  // entry1).
  {
    CriticalActionQueryOptions options;
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].critical_action_id, entry3.critical_action_id);
    EXPECT_EQ(results[1].critical_action_id, entry2.critical_action_id);
    EXPECT_EQ(results[2].critical_action_id, entry1.critical_action_id);
  }

  // Test 2: Filter by begin_time.
  {
    CriticalActionQueryOptions options;
    options.begin_time = base_time - base::Hours(2);
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, entry3.critical_action_id);
    EXPECT_EQ(results[1].critical_action_id, entry2.critical_action_id);
  }

  // Test 3: Filter by end_time.
  {
    CriticalActionQueryOptions options;
    options.end_time = base_time - base::Hours(2);
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].critical_action_id, entry1.critical_action_id);
  }

  // Test 4: Filter by action_types.
  {
    CriticalActionQueryOptions options;
    options.action_types = {ActionType::kFormFill, ActionType::kSettingChange};
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, entry3.critical_action_id);
    EXPECT_EQ(results[1].critical_action_id, entry1.critical_action_id);
  }

  // Test 5: Filter by conversation_id.
  {
    CriticalActionQueryOptions options;
    options.conversation_id = conv_id_1;
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, entry3.critical_action_id);
    EXPECT_EQ(results[1].critical_action_id, entry1.critical_action_id);
  }

  // Test 6: Filter by actor_task_id.
  {
    CriticalActionQueryOptions options;
    options.actor_task_id = task_id_1;
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, entry2.critical_action_id);
    EXPECT_EQ(results[1].critical_action_id, entry1.critical_action_id);
  }

  // Test 7: Filter by max_count.
  {
    CriticalActionQueryOptions options;
    options.max_count = 2;
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, entry3.critical_action_id);
    EXPECT_EQ(results[1].critical_action_id, entry2.critical_action_id);
  }

  // Test 8: Filter by visit_ids.
  {
    CriticalActionQueryOptions options;
    options.visit_ids = {101, 103};
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, entry3.critical_action_id);
    EXPECT_EQ(results[1].critical_action_id, entry1.critical_action_id);
  }

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, GetCriticalActionsBatchesLargeVisitIds) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  base::Time base_time = base::Time::Now();

  // Add entries across three different 500-item batches (1..500, 501..1000,
  // 1001..1200).
  CriticalActionEntry entry_batch1 = CreateDefaultEntry();
  entry_batch1.visit_id = 10;
  entry_batch1.timestamp = base_time - base::Hours(3);
  ASSERT_TRUE(database.AddCriticalAction(entry_batch1));

  CriticalActionEntry entry_batch2 = CreateDefaultEntry();
  entry_batch2.visit_id = 600;
  entry_batch2.timestamp = base_time - base::Hours(1);
  ASSERT_TRUE(database.AddCriticalAction(entry_batch2));

  CriticalActionEntry entry_batch3 = CreateDefaultEntry();
  entry_batch3.visit_id = 1100;
  entry_batch3.timestamp = base_time - base::Hours(2);
  ASSERT_TRUE(database.AddCriticalAction(entry_batch3));

  // Add an entry with a visit_id outside the queried range (1..1200) to verify
  // it is excluded.
  CriticalActionEntry entry_unmatched = CreateDefaultEntry();
  entry_unmatched.visit_id = 2000;
  entry_unmatched.timestamp = base_time;
  ASSERT_TRUE(database.AddCriticalAction(entry_unmatched));

  // Query with 1200 visit IDs (exceeding both kMaxBatchSize=500 and SQLite's
  // 999 parameter limit).
  CriticalActionQueryOptions options;
  options.visit_ids.resize(1200);
  for (size_t i = 0; i < options.visit_ids.size(); ++i) {
    options.visit_ids[i] = static_cast<int64_t>(i + 1);
  }

  {
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].critical_action_id, entry_batch2.critical_action_id);
    EXPECT_EQ(results[1].critical_action_id, entry_batch3.critical_action_id);
    EXPECT_EQ(results[2].critical_action_id, entry_batch1.critical_action_id);
  }

  // Verify max_count is respected after merging and sorting batches.
  {
    options.max_count = 2;
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, entry_batch2.critical_action_id);
    EXPECT_EQ(results[1].critical_action_id, entry_batch3.critical_action_id);
  }

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, MigrationV1ToV2) {
  // 1. Manually set up a V1 database using raw SQLite.
  {
    sql::Database raw_db(sql::Database::Tag("CriticalActions"));
    ASSERT_TRUE(raw_db.Open(db_path_));

    sql::MetaTable meta;
    ASSERT_TRUE(meta.Init(&raw_db, /*version=*/1, /*compatible_version=*/1));

    ASSERT_TRUE(
        raw_db.Execute("CREATE TABLE CriticalActions ("
                       "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                       "  timestamp INTEGER NOT NULL,"
                       "  visit_id INTEGER,"
                       "  conversation_id TEXT,"
                       "  actor_task_id TEXT,"
                       "  action_type INTEGER NOT NULL,"
                       "  url TEXT,"
                       "  metadata TEXT"
                       ")"));

    // Insert 3 records in V1 format:
    ASSERT_TRUE(
        raw_db.Execute("INSERT INTO CriticalActions VALUES ("
                       "  'act_1', 1000000, 42, 'conv_1', 'task_1', 1, "
                       "'https://test.com', 'meta1')"));
    ASSERT_TRUE(
        raw_db.Execute("INSERT INTO CriticalActions VALUES ("
                       "  'act_2', 2000000, 43, NULL, 'task_2', 2, "
                       "'https://test.org', 'meta2')"));
    ASSERT_TRUE(
        raw_db.Execute("INSERT INTO CriticalActions VALUES ("
                       "  'act_3', 3000000, 44, 'conv_2', 'task_3', 3, "
                       "'https://test.io', 'meta3')"));

    raw_db.Close();
  }

  // 2. Open via CriticalActionDatabase, which must auto-migrate to V2.
  {
    CriticalActionDatabase database(db_path_);
    ASSERT_EQ(database.Init(), InitStatus::kOk);

    // Legacy table should be dropped.
    EXPECT_FALSE(database.GetDBForTesting().DoesTableExist("CriticalActions"));

    // New tables should exist.
    EXPECT_TRUE(
        database.GetDBForTesting().DoesTableExist("CriticalActionEntries"));
    EXPECT_TRUE(
        database.GetDBForTesting().DoesTableExist("CriticalActionVisits"));
    EXPECT_TRUE(database.GetDBForTesting().DoesTableExist(
        "CriticalActionConversations"));

    // Verify record A
    auto act_1 = database.GetCriticalAction("act_1");
    ASSERT_TRUE(act_1.has_value());
    EXPECT_EQ(act_1->visit_id, 42);
    EXPECT_EQ(act_1->conversation_id, "conv_1");
    EXPECT_TRUE(act_1->conversation_anchor_id.empty());
    EXPECT_EQ(act_1->actor_task_id, "task_1");
    EXPECT_EQ(act_1->action_type, ActionType::kFormFill);
    EXPECT_EQ(act_1->metadata, "meta1");

    // Verify record B
    auto act_2 = database.GetCriticalAction("act_2");
    ASSERT_TRUE(act_2.has_value());
    EXPECT_EQ(act_2->visit_id, 43);
    EXPECT_TRUE(act_2->conversation_id.empty());
    EXPECT_EQ(act_2->action_type, ActionType::kDownload);

    // Verify record C
    auto act_3 = database.GetCriticalAction("act_3");
    ASSERT_TRUE(act_3.has_value());
    EXPECT_EQ(act_3->visit_id, 44);
    EXPECT_EQ(act_3->conversation_id, "conv_2");
    EXPECT_EQ(act_3->action_type, ActionType::kSettingChange);

    // Verify database version was updated to 4.
    sql::MetaTable meta;
    ASSERT_TRUE(meta.Init(&database.GetDBForTesting(), 4, 3));
    EXPECT_EQ(meta.GetVersionNumber(), 4);
    EXPECT_EQ(meta.GetCompatibleVersionNumber(), 3);
    EXPECT_FALSE(database.GetDBForTesting().DoesColumnExist(
        "CriticalActionEntries", "url"));
    EXPECT_FALSE(database.GetDBForTesting().DoesIndexExist("idx_entries_url"));
    EXPECT_TRUE(database.GetDBForTesting().DoesColumnExist(
        "CriticalActionEntries", "conversation_anchor_id"));

    // Verify GetCriticalActions retrieves all migrated records.
    EXPECT_EQ(database.GetCriticalActions({}).size(), 3u);

    database.Close();
  }
}

TEST_F(CriticalActionDatabaseTest, MigrationV2ToV3) {
  base::HistogramTester histograms;
  {
    sql::Database raw_db(sql::Database::Tag("CriticalActions"));
    ASSERT_TRUE(raw_db.Open(db_path_));
    sql::MetaTable meta;
    ASSERT_TRUE(meta.Init(&raw_db, /*version=*/2, /*compatible_version=*/1));
    // Frozen V2 schema snapshot -- do NOT call InitSchema(), it is V3 now.
    ASSERT_TRUE(raw_db.Execute("CREATE TABLE CriticalActionEntries ("
                               "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                               "  timestamp INTEGER NOT NULL,"
                               "  actor_task_id TEXT,"
                               "  action_type INTEGER NOT NULL,"
                               "  url TEXT,"
                               "  metadata TEXT)"));
    ASSERT_TRUE(raw_db.Execute("CREATE INDEX idx_entries_url ON "
                               "CriticalActionEntries(url)"));
    ASSERT_TRUE(raw_db.Execute("CREATE TABLE CriticalActionVisits ("
                               "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                               "  visit_id INTEGER NOT NULL)"));
    ASSERT_TRUE(raw_db.Execute("CREATE TABLE CriticalActionConversations ("
                               "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                               "  conversation_id TEXT NOT NULL)"));

    ASSERT_TRUE(raw_db.Execute(
        "INSERT INTO CriticalActionEntries VALUES ("
        "  'act_1', 1000000, 'task_1', 1, 'https://test.com', 'meta1')"));
    ASSERT_TRUE(raw_db.Execute(
        "INSERT INTO CriticalActionEntries VALUES ("
        "  'act_2', 2000000, 'task_2', 2, 'https://test.org', 'meta2')"));
    ASSERT_TRUE(raw_db.Execute(
        "INSERT INTO CriticalActionEntries VALUES ("
        "  'act_3', 3000000, 'task_3', 3, 'https://test.io', 'meta3')"));
    ASSERT_TRUE(raw_db.Execute(
        "INSERT INTO CriticalActionVisits VALUES ('act_1', 42)"));
    ASSERT_TRUE(raw_db.Execute(
        "INSERT INTO CriticalActionVisits VALUES ('act_2', 43)"));
    ASSERT_TRUE(raw_db.Execute(
        "INSERT INTO CriticalActionVisits VALUES ('act_3', 44)"));
    raw_db.Close();
  }

  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);
  auto& db = database.GetDBForTesting();

  EXPECT_FALSE(db.DoesColumnExist("CriticalActionEntries", "url"));
  EXPECT_FALSE(db.DoesIndexExist("idx_entries_url"));

  // Non-url data survived, field by field (this is also the ordinal guard).
  auto act_1 = database.GetCriticalAction("act_1");
  ASSERT_TRUE(act_1.has_value());
  EXPECT_EQ(act_1->visit_id, 42);
  EXPECT_EQ(act_1->actor_task_id, "task_1");
  EXPECT_EQ(act_1->metadata, "meta1");
  EXPECT_TRUE(act_1->conversation_anchor_id.empty());
  EXPECT_EQ(database.GetCriticalActions({}).size(), 3u);

  sql::MetaTable meta;
  ASSERT_TRUE(meta.Init(&db, 4, 3));
  EXPECT_EQ(meta.GetVersionNumber(), 4);
  EXPECT_EQ(meta.GetCompatibleVersionNumber(), 3);

  // The one-shot VACUUM ran, succeeded, and left no free pages behind.
  histograms.ExpectUniqueSample(
      "CriticalActions.Database.MigrationVacuumResult", true, 1);
  sql::Statement s(db.GetReadonlyStatement("PRAGMA freelist_count"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(0, s.ColumnInt(0));
}

TEST_F(CriticalActionDatabaseTest, MigrationV3ToV4) {
  base::HistogramTester histograms;
  {
    sql::Database raw_db(sql::Database::Tag("CriticalActions"));
    ASSERT_TRUE(raw_db.Open(db_path_));
    sql::MetaTable meta;
    ASSERT_TRUE(meta.Init(&raw_db, /*version=*/3, /*compatible_version=*/3));
    // Frozen V3 schema snapshot -- do NOT call InitSchema(), it is V4 now.
    ASSERT_TRUE(
        raw_db.Execute("CREATE TABLE CriticalActionEntries ("
                       "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                       "  timestamp INTEGER NOT NULL,"
                       "  actor_task_id TEXT,"
                       "  action_type INTEGER NOT NULL,"
                       "  metadata TEXT)"));
    ASSERT_TRUE(
        raw_db.Execute("CREATE TABLE CriticalActionVisits ("
                       "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                       "  visit_id INTEGER NOT NULL)"));
    ASSERT_TRUE(
        raw_db.Execute("CREATE TABLE CriticalActionConversations ("
                       "  critical_action_id TEXT PRIMARY KEY NOT NULL,"
                       "  conversation_id TEXT NOT NULL)"));

    ASSERT_TRUE(
        raw_db.Execute("INSERT INTO CriticalActionEntries VALUES ("
                       "  'act_1', 1000000, 'task_1', 1, 'meta1')"));
    ASSERT_TRUE(
        raw_db.Execute("INSERT INTO CriticalActionEntries VALUES ("
                       "  'act_2', 2000000, 'task_2', 2, 'meta2')"));
    ASSERT_TRUE(raw_db.Execute(
        "INSERT INTO CriticalActionVisits VALUES ('act_1', 42)"));
    ASSERT_TRUE(raw_db.Execute(
        "INSERT INTO CriticalActionVisits VALUES ('act_2', 43)"));
    ASSERT_TRUE(raw_db.Execute(
        "INSERT INTO CriticalActionConversations VALUES ('act_1', 'conv_1')"));
    raw_db.Close();
  }

  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);
  auto& db = database.GetDBForTesting();

  EXPECT_TRUE(
      db.DoesColumnExist("CriticalActionEntries", "conversation_anchor_id"));

  sql::MetaTable meta;
  ASSERT_TRUE(meta.Init(&db, 4, 3));
  EXPECT_EQ(meta.GetVersionNumber(), 4);
  EXPECT_EQ(meta.GetCompatibleVersionNumber(), 3);
  histograms.ExpectTotalCount("CriticalActions.Database.MigrationVacuumResult",
                              0);

  auto act_1 = database.GetCriticalAction("act_1");
  ASSERT_TRUE(act_1.has_value());
  EXPECT_EQ(act_1->critical_action_id, "act_1");
  EXPECT_EQ(act_1->timestamp, base::Time::FromDeltaSinceWindowsEpoch(
                                  base::Microseconds(1000000)));
  EXPECT_EQ(act_1->visit_id, 42);
  EXPECT_EQ(act_1->conversation_id, "conv_1");
  EXPECT_EQ(act_1->actor_task_id, "task_1");
  EXPECT_EQ(act_1->action_type, ActionType::kFormFill);
  EXPECT_EQ(act_1->metadata, "meta1");
  EXPECT_EQ(act_1->conversation_anchor_id, "");

  auto act_2 = database.GetCriticalAction("act_2");
  ASSERT_TRUE(act_2.has_value());
  EXPECT_EQ(act_2->critical_action_id, "act_2");
  EXPECT_EQ(act_2->timestamp, base::Time::FromDeltaSinceWindowsEpoch(
                                  base::Microseconds(2000000)));
  EXPECT_EQ(act_2->visit_id, 43);
  EXPECT_EQ(act_2->conversation_id, "");
  EXPECT_EQ(act_2->actor_task_id, "task_2");
  EXPECT_EQ(act_2->action_type, ActionType::kDownload);
  EXPECT_EQ(act_2->metadata, "meta2");
  EXPECT_EQ(act_2->conversation_anchor_id, "");

  auto all_entries = database.GetCriticalActions({});
  ASSERT_EQ(all_entries.size(), 2u);
  EXPECT_EQ(all_entries[0], *act_2);
  EXPECT_EQ(all_entries[1], *act_1);

  CriticalActionEntry act_v4 = CreateDefaultEntry();
  act_v4.critical_action_id = "act_v4";
  act_v4.conversation_id = "conv_v4";
  act_v4.conversation_anchor_id = "turn_99";
  ASSERT_TRUE(database.AddCriticalAction(act_v4));

  auto retrieved_v4 = database.GetCriticalAction("act_v4");
  ASSERT_TRUE(retrieved_v4.has_value());
  EXPECT_EQ(retrieved_v4->conversation_anchor_id, "turn_99");
  EXPECT_EQ(*retrieved_v4, act_v4);

  // A new entry without an anchor is stored as NULL too, matching migrated
  // rows.
  CriticalActionEntry act_v4_no_anchor = CreateDefaultEntry();
  act_v4_no_anchor.critical_action_id = "act_v4_no_anchor";
  ASSERT_TRUE(database.AddCriticalAction(act_v4_no_anchor));

  // Check the raw column: migrated rows and the anchorless v4 row must be
  // NULL (not ''), and only `act_v4` has a non-NULL anchor.
  {
    sql::Statement null_stmt(db.GetReadonlyStatement(
        "SELECT critical_action_id FROM CriticalActionEntries "
        "WHERE conversation_anchor_id IS NULL ORDER BY critical_action_id"));
    std::vector<std::string> null_anchor_ids;
    while (null_stmt.Step()) {
      null_anchor_ids.push_back(null_stmt.ColumnString(0));
    }
    ASSERT_TRUE(null_stmt.Succeeded());
    EXPECT_THAT(null_anchor_ids,
                testing::ElementsAre("act_1", "act_2", "act_v4_no_anchor"));
  }
  {
    sql::Statement empty_stmt(
        db.GetReadonlyStatement("SELECT COUNT(*) FROM CriticalActionEntries "
                                "WHERE conversation_anchor_id = ''"));
    ASSERT_TRUE(empty_stmt.Step());
    EXPECT_EQ(0, empty_stmt.ColumnInt(0));
  }

  database.Close();
}

// Version 4 keeps kCompatibleVersion = 3, so the exact statements a v3 client
// runs must keep working against a v4 database. If this test fails after a
// schema change, bump kCompatibleVersion.
TEST_F(CriticalActionDatabaseTest, V3StatementsWorkOnV4Database) {
  const base::Time v3_timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000000));

  CriticalActionEntry v4_entry = CreateDefaultEntry();
  v4_entry.critical_action_id = "act_v4";
  v4_entry.visit_id = 43;
  v4_entry.conversation_id = "conv_v4";
  v4_entry.conversation_anchor_id = "turn_v4";
  v4_entry.actor_task_id = "task_v4";
  v4_entry.metadata = "meta_v4";
  {
    CriticalActionDatabase database(db_path_);
    ASSERT_EQ(database.Init(), InitStatus::kOk);
    ASSERT_TRUE(database.AddCriticalAction(v4_entry));
    database.Close();
  }

  {
    sql::Database raw_db(sql::Database::Tag("CriticalActions"));
    ASSERT_TRUE(raw_db.Open(db_path_));
    sql::MetaTable meta;
    ASSERT_TRUE(meta.Init(&raw_db, /*version=*/4, /*compatible_version=*/3));
    EXPECT_EQ(meta.GetVersionNumber(), 4);
    EXPECT_EQ(meta.GetCompatibleVersionNumber(), 3);

    // v3 CriticalActionDatabase::AddCriticalAction() statements.
    sql::Statement insert_entry(raw_db.GetUniqueStatement(
        "INSERT INTO CriticalActionEntries (critical_action_id, timestamp, "
        "action_type, metadata, actor_task_id) "
        "VALUES (?, ?, ?, ?, ?)"));
    insert_entry.BindString(0, "act_v3");
    insert_entry.BindTime(1, v3_timestamp);
    insert_entry.BindInt(2, static_cast<int>(ActionType::kDownload));
    insert_entry.BindString(3, "meta_v3");
    insert_entry.BindString(4, "task_v3");
    ASSERT_TRUE(insert_entry.Run());

    sql::Statement insert_visit(raw_db.GetUniqueStatement(
        "INSERT INTO CriticalActionVisits (critical_action_id, "
        "visit_id) VALUES (?, ?)"));
    insert_visit.BindString(0, "act_v3");
    insert_visit.BindInt64(1, 42);
    ASSERT_TRUE(insert_visit.Run());

    sql::Statement insert_conv(raw_db.GetUniqueStatement(
        "INSERT INTO CriticalActionConversations "
        "(critical_action_id, conversation_id) VALUES (?, ?)"));
    insert_conv.BindString(0, "act_v3");
    insert_conv.BindString(1, "conv_v3");
    ASSERT_TRUE(insert_conv.Run());

    // v3 CriticalActionDatabase::GetCriticalAction() statement, run against
    // a row written by v4.
    sql::Statement select(raw_db.GetUniqueStatement(
        "SELECT e.critical_action_id, e.timestamp, v.visit_id, "
        "c.conversation_id, "
        "       e.actor_task_id, e.action_type, e.metadata "
        "FROM CriticalActionEntries e "
        "LEFT JOIN CriticalActionVisits v ON e.critical_action_id = "
        "v.critical_action_id "
        "LEFT JOIN CriticalActionConversations c ON e.critical_action_id = "
        "c.critical_action_id "
        "WHERE e.critical_action_id = ?"));
    select.BindString(0, "act_v4");
    ASSERT_TRUE(select.Step());
    EXPECT_EQ(select.ColumnString(0), "act_v4");
    EXPECT_EQ(select.ColumnTime(1), v4_entry.timestamp);
    EXPECT_EQ(select.ColumnInt64(2), 43);
    EXPECT_EQ(select.ColumnString(3), "conv_v4");
    EXPECT_EQ(select.ColumnString(4), "task_v4");
    EXPECT_EQ(select.ColumnInt(5), static_cast<int>(v4_entry.action_type));
    EXPECT_EQ(select.ColumnString(6), "meta_v4");
    EXPECT_FALSE(select.Step());
    EXPECT_TRUE(select.Succeeded());
    // `raw_db` is closed when it goes out of scope, after the statements.
  }

  // The row written by the v3 statements reads back in v4 with an empty
  // anchor (NULL column), and the v4 row keeps its anchor.
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);
  auto act_v3 = database.GetCriticalAction("act_v3");
  ASSERT_TRUE(act_v3.has_value());
  EXPECT_EQ(act_v3->timestamp, v3_timestamp);
  EXPECT_EQ(act_v3->visit_id, 42);
  EXPECT_EQ(act_v3->conversation_id, "conv_v3");
  EXPECT_EQ(act_v3->actor_task_id, "task_v3");
  EXPECT_EQ(act_v3->action_type, ActionType::kDownload);
  EXPECT_EQ(act_v3->metadata, "meta_v3");
  EXPECT_EQ(act_v3->conversation_anchor_id, "");

  auto act_v4 = database.GetCriticalAction("act_v4");
  ASSERT_TRUE(act_v4.has_value());
  EXPECT_EQ(*act_v4, v4_entry);

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, FreshDatabaseDoesNotVacuum) {
  base::HistogramTester histograms;
  {
    CriticalActionDatabase database(db_path_);
    ASSERT_EQ(database.Init(), InitStatus::kOk);
    histograms.ExpectTotalCount(
        "CriticalActions.Database.MigrationVacuumResult", 0);
  }
  {
    CriticalActionDatabase database(db_path_);
    ASSERT_EQ(database.Init(), InitStatus::kOk);
    histograms.ExpectTotalCount(
        "CriticalActions.Database.MigrationVacuumResult", 0);
  }
}

TEST_F(CriticalActionDatabaseTest, SchemaHasNoUrlColumn) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);
  auto& db = database.GetDBForTesting();
  EXPECT_FALSE(db.DoesColumnExist("CriticalActionEntries", "url"));
  EXPECT_FALSE(db.DoesIndexExist("idx_entries_url"));
  static constexpr base::cstring_view kExpectedColumns[] = {
      "critical_action_id", "timestamp", "actor_task_id",
      "action_type",        "metadata",  "conversation_anchor_id"};
  for (base::cstring_view column : kExpectedColumns) {
    EXPECT_TRUE(db.DoesColumnExist("CriticalActionEntries", column)) << column;
  }
}

TEST(CriticalActionDatabaseHelpersTest, CreatePlaceholders) {
  EXPECT_EQ(CriticalActionDatabase::CreatePlaceholders(0), "");
  EXPECT_EQ(CriticalActionDatabase::CreatePlaceholders(1), "?");
  EXPECT_EQ(CriticalActionDatabase::CreatePlaceholders(2), "?,?");
}

TEST(CriticalActionDatabaseHelpersTest, BuildInCondition) {
  EXPECT_EQ(CriticalActionDatabase::BuildInCondition("visit_id", 0), "");
  EXPECT_EQ(CriticalActionDatabase::BuildInCondition("visit_id", 1),
            "visit_id IN (?)");
  EXPECT_EQ(CriticalActionDatabase::BuildInCondition("e.action_type", 2),
            "e.action_type IN (?,?)");
  EXPECT_EQ(CriticalActionDatabase::BuildInCondition("v.visit_id", 3),
            "v.visit_id IN (?,?,?)");
}

TEST(CriticalActionDatabaseHelpersTest, AddTimeRangeConditions) {
  std::vector<std::string> conditions;
  CriticalActionDatabase::AddTimeRangeConditions(conditions, "e.timestamp",
                                                 std::nullopt, std::nullopt);
  EXPECT_TRUE(conditions.empty());

  base::Time t1 = base::Time::FromTimeT(1000);
  base::Time t2 = base::Time::FromTimeT(2000);

  CriticalActionDatabase::AddTimeRangeConditions(conditions, "e.timestamp", t1,
                                                 std::nullopt);
  ASSERT_EQ(conditions.size(), 1u);
  EXPECT_EQ(conditions[0], "e.timestamp >= ?");

  conditions.clear();
  CriticalActionDatabase::AddTimeRangeConditions(conditions, "e.timestamp",
                                                 std::nullopt, t2);
  ASSERT_EQ(conditions.size(), 1u);
  EXPECT_EQ(conditions[0], "e.timestamp < ?");

  conditions.clear();
  CriticalActionDatabase::AddTimeRangeConditions(conditions, "e.timestamp", t1,
                                                 t2);
  ASSERT_EQ(conditions.size(), 2u);
  EXPECT_EQ(conditions[0], "e.timestamp >= ?");
  EXPECT_EQ(conditions[1], "e.timestamp < ?");
}

TEST(CriticalActionDatabaseHelpersTest,
     BuildGetCriticalActionsQuery_DefaultOptions) {
  CriticalActionQueryOptions options;
  std::string query = CriticalActionDatabase::BuildGetCriticalActionsQuery(
      options, /*batch_visit_ids_count=*/0);
  EXPECT_THAT(query, HasSubstr("SELECT e.critical_action_id"));
  EXPECT_THAT(query, HasSubstr("e.conversation_anchor_id"));
  EXPECT_THAT(query, Not(HasSubstr("e.url")));
  EXPECT_THAT(query, Not(HasSubstr("WHERE")));
  EXPECT_THAT(query, HasSubstr("ORDER BY e.timestamp DESC"));
  EXPECT_THAT(query, Not(HasSubstr("LIMIT")));
}

TEST(CriticalActionDatabaseHelpersTest,
     BuildGetCriticalActionsQuery_TimeRange) {
  CriticalActionQueryOptions options;
  options.begin_time = base::Time::FromTimeT(1000);
  options.end_time = base::Time::FromTimeT(2000);
  std::string query = CriticalActionDatabase::BuildGetCriticalActionsQuery(
      options, /*batch_visit_ids_count=*/0);
  EXPECT_THAT(query, HasSubstr("WHERE e.timestamp >= ? AND e.timestamp < ?"));
  EXPECT_THAT(query, HasSubstr("ORDER BY e.timestamp DESC"));
  EXPECT_THAT(query, Not(HasSubstr("LIMIT")));
}

TEST(CriticalActionDatabaseHelpersTest,
     BuildGetCriticalActionsQuery_WithFiltersAndLimit) {
  CriticalActionQueryOptions options;
  options.action_types = {ActionType::kFormFill, ActionType::kDownload};
  options.visit_ids = {101, 102, 103};
  options.conversation_id = "test_conversation";
  options.actor_task_id = "test_task";
  options.max_count = 50;

  std::string query = CriticalActionDatabase::BuildGetCriticalActionsQuery(
      options, options.visit_ids.size());
  EXPECT_THAT(query, HasSubstr("WHERE e.action_type IN (?,?) AND "
                               "v.visit_id IN (?,?,?) AND "
                               "c.conversation_id = ? AND "
                               "e.actor_task_id = ?"));
  EXPECT_THAT(query, HasSubstr("ORDER BY e.timestamp DESC LIMIT ?"));
}

TEST_F(CriticalActionDatabaseTest, SetCriticalActionsConversationId) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  const std::string action_id1 = "action_1";
  const std::string action_id2 = "action_2";
  const std::string conv_id = "conv_1";

  CriticalActionEntry entry1 = CreateDefaultEntry();
  entry1.critical_action_id = action_id1;
  ASSERT_TRUE(database.AddCriticalAction(entry1));

  CriticalActionEntry entry2 = CreateDefaultEntry();
  entry2.critical_action_id = action_id2;
  ASSERT_TRUE(database.AddCriticalAction(entry2));

  // Before associating conversation ID, conversation_id is empty.
  auto retrieved1 = database.GetCriticalAction(action_id1);
  ASSERT_TRUE(retrieved1.has_value());
  EXPECT_TRUE(retrieved1->conversation_id.empty());

  // Associate conversation ID with action_id1.
  EXPECT_TRUE(database.SetCriticalActionsConversationId({action_id1}, conv_id));

  retrieved1 = database.GetCriticalAction(action_id1);
  ASSERT_TRUE(retrieved1.has_value());
  EXPECT_EQ(retrieved1->conversation_id, conv_id);

  // action_id2 remains without conversation ID.
  auto retrieved2 = database.GetCriticalAction(action_id2);
  ASSERT_TRUE(retrieved2.has_value());
  EXPECT_TRUE(retrieved2->conversation_id.empty());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest,
       SetCriticalActionsConversationIdEmptyActionIds) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  // Empty action IDs is a no-op that returns true.
  EXPECT_TRUE(database.SetCriticalActionsConversationId({}, "conv_1"));

  database.Close();
}

TEST_F(CriticalActionDatabaseTest,
       SetCriticalActionsConversationIdEmptyConversationId) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  const std::string action_id = "action_1";
  CriticalActionEntry entry = CreateDefaultEntry();
  entry.critical_action_id = action_id;
  ASSERT_TRUE(database.AddCriticalAction(entry));

  // Empty conversation ID returns false.
  EXPECT_FALSE(database.SetCriticalActionsConversationId({action_id}, ""));

  database.Close();
}

// SetCriticalActionsConversationId() uses INSERT OR REPLACE on
// CriticalActionConversations. Replacing an existing conversation_id must not
// clobber the anchor stored on CriticalActionEntries.
TEST_F(CriticalActionDatabaseTest,
       SetCriticalActionsConversationIdPreservesConversationAnchorId) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  CriticalActionEntry entry = CreateDefaultEntry();
  entry.conversation_id = "initial_conv";
  entry.conversation_anchor_id = "anchor_abc";
  ASSERT_TRUE(database.AddCriticalAction(entry));

  ASSERT_TRUE(database.SetCriticalActionsConversationId(
      {entry.critical_action_id}, "backfilled_conv"));
  auto updated = database.GetCriticalAction(entry.critical_action_id);
  ASSERT_TRUE(updated.has_value());
  EXPECT_EQ(updated->conversation_id, "backfilled_conv");
  EXPECT_EQ(updated->conversation_anchor_id, "anchor_abc");

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, ConversationAnchorIdMaxLengthGuard) {
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  const base::Time base_time = base::Time::Now();
  const std::string max_len_anchor(kMaxConversationAnchorIdLength, 'a');
  const std::string oversized_anchor(kMaxConversationAnchorIdLength + 1, 'b');

  CriticalActionEntry valid_entry = CreateDefaultEntry();
  valid_entry.timestamp = base_time - base::Minutes(1);
  valid_entry.visit_id = 201;
  valid_entry.conversation_id = "conv_valid";
  valid_entry.conversation_anchor_id = max_len_anchor;
  valid_entry.actor_task_id = "task_valid";
  valid_entry.action_type = ActionType::kFormFill;
  valid_entry.metadata = R"({"status":"ok"})";
  ASSERT_TRUE(database.AddCriticalAction(valid_entry));

  auto retrieved_valid =
      database.GetCriticalAction(valid_entry.critical_action_id);
  ASSERT_TRUE(retrieved_valid.has_value());
  EXPECT_EQ(retrieved_valid->conversation_anchor_id, max_len_anchor);
  EXPECT_EQ(*retrieved_valid, valid_entry);

  CriticalActionEntry oversized_entry = CreateDefaultEntry();
  oversized_entry.timestamp = base_time;
  oversized_entry.visit_id = 202;
  oversized_entry.conversation_id = "conv_oversized";
  oversized_entry.conversation_anchor_id = oversized_anchor;
  oversized_entry.actor_task_id = "task_oversized";
  oversized_entry.action_type = ActionType::kDownload;
  oversized_entry.metadata = R"({"status":"dropped_anchor"})";
  ASSERT_TRUE(database.AddCriticalAction(oversized_entry));

  auto retrieved_oversized =
      database.GetCriticalAction(oversized_entry.critical_action_id);
  ASSERT_TRUE(retrieved_oversized.has_value());
  EXPECT_EQ(retrieved_oversized->visit_id, oversized_entry.visit_id);
  EXPECT_EQ(retrieved_oversized->conversation_id,
            oversized_entry.conversation_id);
  EXPECT_EQ(retrieved_oversized->actor_task_id, oversized_entry.actor_task_id);
  EXPECT_EQ(retrieved_oversized->action_type, oversized_entry.action_type);
  EXPECT_EQ(retrieved_oversized->metadata, oversized_entry.metadata);
  EXPECT_TRUE(retrieved_oversized->conversation_anchor_id.empty());

  auto all_entries = database.GetCriticalActions({});
  ASSERT_EQ(all_entries.size(), 2u);
  EXPECT_EQ(all_entries[0].critical_action_id,
            oversized_entry.critical_action_id);
  EXPECT_EQ(all_entries[0].visit_id, oversized_entry.visit_id);
  EXPECT_EQ(all_entries[0].conversation_id, oversized_entry.conversation_id);
  EXPECT_EQ(all_entries[0].actor_task_id, oversized_entry.actor_task_id);
  EXPECT_EQ(all_entries[0].action_type, oversized_entry.action_type);
  EXPECT_EQ(all_entries[0].metadata, oversized_entry.metadata);
  EXPECT_TRUE(all_entries[0].conversation_anchor_id.empty());
  EXPECT_EQ(all_entries[1], valid_entry);

  database.Close();
}

TEST_F(CriticalActionDatabaseTest,
       SecondConnectionDoesNotDeadlockExistingDatabase) {
  // First create the database at the current schema version and close it, so
  // that the subsequent Init() opens an already-initialized database where no
  // schema or migration writes occur.
  {
    CriticalActionDatabase setup_db(db_path_);
    ASSERT_EQ(setup_db.Init(), InitStatus::kOk);
    setup_db.Close();
  }

  base::HistogramTester histogram_tester;
  CriticalActionDatabase db1(db_path_);
  ASSERT_EQ(db1.Init(), InitStatus::kOk);

  // Attempting to initialize a second connection while `db1` is open must fail
  // fast during Init() rather than acquiring a second SHARED lock that
  // deadlocks subsequent writes on `db1`.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_BUSY);

    CriticalActionDatabase db2(db_path_);
    EXPECT_EQ(db2.Init(), InitStatus::kBusyTimeout);
    EXPECT_FALSE(db2.GetDBForTesting().is_open());
    EXPECT_FALSE(db2.GetDBForTesting().has_error_callback());
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 1);

  // Verify `db1` can still perform writes and reads without hitting
  // SQLITE_BUSY.
  CriticalActionEntry entry = CreateDefaultEntry();
  entry.visit_id = 42;
  EXPECT_TRUE(db1.AddCriticalAction(entry));

  auto retrieved = db1.GetCriticalAction(entry.critical_action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);

  EXPECT_TRUE(db1.DeleteCriticalActionsByVisitIds({42}));
  EXPECT_FALSE(db1.GetCriticalAction(entry.critical_action_id).has_value());

  // No additional errors should have been recorded on `db1`.
  histogram_tester.ExpectUniqueSample("CriticalActions.Database.SqliteError",
                                      sql::SqliteLoggedResultCode::kBusy, 1);

  db1.Close();
}

TEST_F(CriticalActionDatabaseTest,
       TransientLockDuringInitSucceedsWithoutLoggingBusyError) {
  {
    CriticalActionDatabase setup_db(db_path_);
    ASSERT_EQ(setup_db.Init(), InitStatus::kOk);
    setup_db.Close();
  }

  base::HistogramTester histogram_tester;
  base::Thread holder_thread("LockHolder");
  ASSERT_TRUE(holder_thread.Start());

  std::unique_ptr<sql::Database> holder_db;
  bool holder_opened = false;
  base::WaitableEvent holder_locked;
  holder_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        holder_db = std::make_unique<sql::Database>(
            sql::Database::Tag("CriticalActions"));
        holder_opened = holder_db->Open(db_path_);
        if (!holder_opened) {
          holder_db.reset();
        }
        holder_locked.Signal();
      }));

  holder_locked.Wait();
  ASSERT_TRUE(holder_opened);

  int busy_retries = 0;
  CriticalActionDatabase db2(db_path_);
  db2.SetBusyRetryCallbackForTesting(base::BindLambdaForTesting([&] {
    if (++busy_retries == 1) {
      holder_thread.task_runner()->PostTask(FROM_HERE,
                                            base::BindLambdaForTesting([&] {
                                              holder_db->Close();
                                              holder_db.reset();
                                            }));
    }
  }));

  EXPECT_EQ(db2.Init(), InitStatus::kOk);

  holder_thread.Stop();

  EXPECT_GE(busy_retries, 1);
  histogram_tester.ExpectTotalCount("CriticalActions.Database.SqliteError", 0);

  CriticalActionEntry entry = CreateDefaultEntry();
  EXPECT_TRUE(db2.AddCriticalAction(entry));
  EXPECT_TRUE(db2.GetCriticalAction(entry.critical_action_id).has_value());

  db2.Close();
}

TEST_F(CriticalActionDatabaseTest,
       NonBusyOpenFailureFailsFastAndClearsErrorCallback) {
  base::HistogramTester histogram_tester;

  // Point `db_path_` at a directory so `db_.Open()` fails with a non-busy
  // error (`SQLITE_CANTOPEN`) on the first attempt without retrying.
  CriticalActionDatabase db(temp_dir_.GetPath());
  EXPECT_EQ(db.Init(), InitStatus::kError);
  EXPECT_FALSE(db.GetDBForTesting().is_open());
  EXPECT_FALSE(db.GetDBForTesting().has_error_callback());

  histogram_tester.ExpectTotalCount(
      "Sql.Database.Open.FirstAttempt.Error.CriticalActions", 1);
  histogram_tester.ExpectBucketCount("CriticalActions.Database.SqliteError",
                                     sql::SqliteLoggedResultCode::kBusy, 0);
  EXPECT_FALSE(
      histogram_tester.GetAllSamples("CriticalActions.Database.SqliteError")
          .empty());

  // Calling `Init()` again on the same instance must also fail cleanly without
  // invoking a dangling error callback.
  EXPECT_EQ(db.Init(), InitStatus::kError);
  EXPECT_FALSE(db.GetDBForTesting().is_open());
  EXPECT_FALSE(db.GetDBForTesting().has_error_callback());
}

TEST_F(CriticalActionDatabaseTest, ErrorCallbackRestoredAfterInit) {
  base::HistogramTester histogram_tester;
  CriticalActionDatabase database(db_path_);
  ASSERT_EQ(database.Init(), InitStatus::kOk);

  CriticalActionEntry entry = CreateDefaultEntry();
  ASSERT_TRUE(database.AddCriticalAction(entry));

  // Verify non-busy statement errors record
  // `CriticalActions.Database.SqliteError`.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CONSTRAINT_PRIMARYKEY);
    EXPECT_FALSE(database.AddCriticalAction(entry));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  histogram_tester.ExpectBucketCount(
      "CriticalActions.Database.SqliteError",
      sql::SqliteLoggedResultCode::kConstraintPrimaryKey, 1);

  // Switch `database` to NORMAL locking for testing so another connection can
  // hold an EXCLUSIVE lock, then verify that a post-Init() `SQLITE_BUSY` error
  // also invokes `DatabaseErrorCallback` rather than being swallowed by
  // Init()'s temporary callback.
  ASSERT_TRUE(database.GetDBForTesting().Execute("PRAGMA locking_mode=NORMAL"));
  ASSERT_TRUE(database.GetDBForTesting().Execute("BEGIN IMMEDIATE"));
  ASSERT_TRUE(database.GetDBForTesting().Execute("COMMIT"));

  sql::Database raw_locker(sql::Database::Tag("CriticalActions"));
  ASSERT_TRUE(raw_locker.Open(db_path_));
  ASSERT_TRUE(raw_locker.Execute("BEGIN EXCLUSIVE"));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_BUSY);
    EXPECT_FALSE(database.AddCriticalAction(CreateDefaultEntry()));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
  histogram_tester.ExpectBucketCount("CriticalActions.Database.SqliteError",
                                     sql::SqliteLoggedResultCode::kBusy, 1);

  raw_locker.Close();
  database.Close();
}

}  // namespace critical_actions
