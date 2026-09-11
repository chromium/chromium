// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_database.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "sql/sqlite_result_code.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace critical_actions {

class CriticalActionDatabaseTest : public testing::Test {
 public:
  CriticalActionDatabaseTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("TestCriticalActions.db");
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

TEST_F(CriticalActionDatabaseTest, InitDatabase) {
  CriticalActionDatabase database(db_path_);
  EXPECT_TRUE(database.Init());
  EXPECT_TRUE(base::PathExists(db_path_));
  database.Close();
}

TEST_F(CriticalActionDatabaseTest, AddAndGetEntry) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  const std::string action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry;
  entry.critical_action_id = action_id;
  entry.timestamp = base::Time::Now();
  entry.visit_id = base::RandIntInclusive(1, 1000000);
  entry.conversation_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.actor_task_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.action_type = ActionType::kFormFill;
  entry.url = GURL("https://example.com/login");
  entry.metadata = "{\"key\": \"val\"}";

  EXPECT_TRUE(database.AddCriticalAction(entry));

  auto retrieved = database.GetCriticalAction(action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, AddDuplicateEntryFails) {
  base::HistogramTester histogram_tester;
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  CriticalActionEntry entry;
  entry.critical_action_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.timestamp = base::Time::Now();
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
  ASSERT_TRUE(database.Init());

  auto retrieved = database.GetCriticalAction(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  EXPECT_FALSE(retrieved.has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, DeleteSingleEntry) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  const std::string action_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry;
  entry.critical_action_id = action_id;
  entry.timestamp = base::Time::Now();
  entry.action_type = ActionType::kSettingChange;

  EXPECT_TRUE(database.AddCriticalAction(entry));
  EXPECT_TRUE(database.DeleteCriticalAction(action_id));

  auto retrieved = database.GetCriticalAction(action_id);
  EXPECT_FALSE(retrieved.has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, DeleteInTimeRange) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  base::Time base_time = base::Time::Now();

  const std::string action_id_1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry1;
  entry1.critical_action_id = action_id_1;
  entry1.timestamp = base_time - base::Hours(2);
  entry1.action_type = ActionType::kFormFill;
  ASSERT_TRUE(database.AddCriticalAction(entry1));

  const std::string action_id_2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry2;
  entry2.critical_action_id = action_id_2;
  entry2.timestamp = base_time;
  entry2.action_type = ActionType::kFormFill;
  ASSERT_TRUE(database.AddCriticalAction(entry2));

  const std::string action_id_3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry3;
  entry3.critical_action_id = action_id_3;
  entry3.timestamp = base_time + base::Hours(2);
  entry3.action_type = ActionType::kFormFill;
  ASSERT_TRUE(database.AddCriticalAction(entry3));

  // Delete everything around the middle entry (base_time).
  // Time range is inclusive of start, exclusive of end.
  // Start from -1 hour to +1 hour.
  EXPECT_TRUE(database.DeleteCriticalActionsInTimeRange(
      base_time - base::Hours(1), base_time + base::Hours(1)));

  // entry1 should remain (2 hours ago)
  EXPECT_TRUE(database.GetCriticalAction(action_id_1).has_value());
  // entry2 should have been deleted (exactly base_time)
  EXPECT_FALSE(database.GetCriticalAction(action_id_2).has_value());
  // entry3 should remain (2 hours from now)
  EXPECT_TRUE(database.GetCriticalAction(action_id_3).has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, DeleteByVisitIds) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  int64_t visit_id_1 = base::RandIntInclusive(1, 1000000);
  int64_t visit_id_2 = visit_id_1 + 1;
  int64_t visit_id_3 = visit_id_1 + 2;

  const std::string action_id_1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry1;
  entry1.critical_action_id = action_id_1;
  entry1.visit_id = visit_id_1;
  entry1.action_type = ActionType::kFormFill;
  ASSERT_TRUE(database.AddCriticalAction(entry1));

  const std::string action_id_2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry2;
  entry2.critical_action_id = action_id_2;
  entry2.visit_id = visit_id_2;
  entry2.action_type = ActionType::kDownload;
  ASSERT_TRUE(database.AddCriticalAction(entry2));

  const std::string action_id_3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry3;
  entry3.critical_action_id = action_id_3;
  entry3.visit_id = visit_id_3;
  entry3.action_type = ActionType::kSettingChange;
  ASSERT_TRUE(database.AddCriticalAction(entry3));

  // Deleting visit_id_1 and visit_id_3.
  EXPECT_TRUE(
      database.DeleteCriticalActionsByVisitIds({visit_id_1, visit_id_3}));

  // entry1 and entry3 should be deleted, entry2 should remain.
  EXPECT_FALSE(database.GetCriticalAction(action_id_1).has_value());
  EXPECT_TRUE(database.GetCriticalAction(action_id_2).has_value());
  EXPECT_FALSE(database.GetCriticalAction(action_id_3).has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, GetCriticalActionsWithOptions) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  base::Time base_time = base::Time::Now();

  const std::string conv_id_1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string conv_id_2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string task_id_1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string task_id_2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  const std::string action_id_1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry1;
  entry1.critical_action_id = action_id_1;
  entry1.timestamp = base_time - base::Hours(3);
  entry1.action_type = ActionType::kFormFill;
  entry1.conversation_id = conv_id_1;
  entry1.actor_task_id = task_id_1;
  entry1.visit_id = 101;
  entry1.url = GURL("https://example.com/page1");
  ASSERT_TRUE(database.AddCriticalAction(entry1));

  const std::string action_id_2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry2;
  entry2.critical_action_id = action_id_2;
  entry2.timestamp = base_time - base::Hours(2);
  entry2.action_type = ActionType::kDownload;
  entry2.conversation_id = conv_id_2;
  entry2.actor_task_id = task_id_1;
  entry2.visit_id = 102;
  entry2.url = GURL("https://example.org/page2");
  ASSERT_TRUE(database.AddCriticalAction(entry2));

  const std::string action_id_3 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  CriticalActionEntry entry3;
  entry3.critical_action_id = action_id_3;
  entry3.timestamp = base_time - base::Hours(1);
  entry3.action_type = ActionType::kSettingChange;
  entry3.conversation_id = conv_id_1;
  entry3.actor_task_id = task_id_2;
  entry3.visit_id = 103;
  entry3.url = GURL("https://example.com/page3");
  ASSERT_TRUE(database.AddCriticalAction(entry3));

  // Test 1: Query all, verify order (timestamp DESC: entry3 -> entry2 ->
  // entry1).
  {
    CriticalActionQueryOptions options;
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].critical_action_id, action_id_3);
    EXPECT_EQ(results[1].critical_action_id, action_id_2);
    EXPECT_EQ(results[2].critical_action_id, action_id_1);
  }

  // Test 2: Filter by begin_time.
  {
    CriticalActionQueryOptions options;
    options.begin_time = base_time - base::Hours(2);
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, action_id_3);
    EXPECT_EQ(results[1].critical_action_id, action_id_2);
  }

  // Test 3: Filter by end_time.
  {
    CriticalActionQueryOptions options;
    options.end_time = base_time - base::Hours(2);
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].critical_action_id, action_id_1);
  }

  // Test 4: Filter by action_types.
  {
    CriticalActionQueryOptions options;
    options.action_types = {ActionType::kFormFill, ActionType::kSettingChange};
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, action_id_3);
    EXPECT_EQ(results[1].critical_action_id, action_id_1);
  }

  // Test 5: Filter by conversation_id.
  {
    CriticalActionQueryOptions options;
    options.conversation_id = conv_id_1;
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, action_id_3);
    EXPECT_EQ(results[1].critical_action_id, action_id_1);
  }

  // Test 6: Filter by actor_task_id.
  {
    CriticalActionQueryOptions options;
    options.actor_task_id = task_id_1;
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, action_id_2);
    EXPECT_EQ(results[1].critical_action_id, action_id_1);
  }

  // Test 7: Filter by max_count.
  {
    CriticalActionQueryOptions options;
    options.max_count = 2;
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, action_id_3);
    EXPECT_EQ(results[1].critical_action_id, action_id_2);
  }

  // Test 8: Filter by visit_ids.
  {
    CriticalActionQueryOptions options;
    options.visit_ids = {101, 103};
    auto results = database.GetCriticalActions(options);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].critical_action_id, action_id_3);
    EXPECT_EQ(results[1].critical_action_id, action_id_1);
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
    ASSERT_TRUE(database.Init());

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
    EXPECT_EQ(act_1->actor_task_id, "task_1");
    EXPECT_EQ(act_1->action_type, ActionType::kFormFill);
    EXPECT_EQ(act_1->url, GURL("https://test.com"));
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

    // Verify database version was updated to 2.
    sql::MetaTable meta;
    ASSERT_TRUE(meta.Init(&database.GetDBForTesting(), 2, 1));
    EXPECT_EQ(meta.GetVersionNumber(), 2);

    // Verify GetCriticalActions retrieves all migrated records.
    EXPECT_EQ(database.GetCriticalActions({}).size(), 3u);

    database.Close();
  }
}

}  // namespace critical_actions
