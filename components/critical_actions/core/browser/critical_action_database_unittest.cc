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
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace critical_actions {

using ::testing::HasSubstr;
using ::testing::Not;

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
  EXPECT_TRUE(database.Init());
  EXPECT_TRUE(base::PathExists(db_path_));
  database.Close();
}

TEST_F(CriticalActionDatabaseTest, AddAndGetEntry) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  CriticalActionEntry entry = CreateDefaultEntry();
  entry.visit_id = base::RandIntInclusive(1, 1000000);
  entry.conversation_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.actor_task_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.metadata = "{\"key\": \"val\"}";

  EXPECT_TRUE(database.AddCriticalAction(entry));

  auto retrieved = database.GetCriticalAction(entry.critical_action_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(*retrieved, entry);
  EXPECT_EQ(retrieved->critical_action_id, entry.critical_action_id);
  EXPECT_EQ(retrieved->actor_task_id, entry.actor_task_id);
  EXPECT_EQ(retrieved->conversation_id, entry.conversation_id);
  EXPECT_EQ(retrieved->metadata, entry.metadata);

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, AddDuplicateEntryFails) {
  base::HistogramTester histogram_tester;
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

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
  ASSERT_TRUE(database.Init());

  auto retrieved = database.GetCriticalAction(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  EXPECT_FALSE(retrieved.has_value());

  database.Close();
}

TEST_F(CriticalActionDatabaseTest, DeleteSingleEntry) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

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
  ASSERT_TRUE(database.Init());

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
  ASSERT_TRUE(database.Init());

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

    // Verify database version was updated to 3.
    sql::MetaTable meta;
    ASSERT_TRUE(meta.Init(&database.GetDBForTesting(), 3, 3));
    EXPECT_EQ(meta.GetVersionNumber(), 3);
    EXPECT_EQ(meta.GetCompatibleVersionNumber(), 3);
    EXPECT_FALSE(database.GetDBForTesting().DoesColumnExist(
        "CriticalActionEntries", "url"));
    EXPECT_FALSE(database.GetDBForTesting().DoesIndexExist("idx_entries_url"));

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
  ASSERT_TRUE(database.Init());
  auto& db = database.GetDBForTesting();

  EXPECT_FALSE(db.DoesColumnExist("CriticalActionEntries", "url"));
  EXPECT_FALSE(db.DoesIndexExist("idx_entries_url"));

  // Non-url data survived, field by field (this is also the ordinal guard).
  auto act_1 = database.GetCriticalAction("act_1");
  ASSERT_TRUE(act_1.has_value());
  EXPECT_EQ(act_1->visit_id, 42);
  EXPECT_EQ(act_1->actor_task_id, "task_1");
  EXPECT_EQ(act_1->metadata, "meta1");
  EXPECT_EQ(database.GetCriticalActions({}).size(), 3u);

  sql::MetaTable meta;
  ASSERT_TRUE(meta.Init(&db, 3, 3));
  EXPECT_EQ(meta.GetVersionNumber(), 3);
  EXPECT_EQ(meta.GetCompatibleVersionNumber(), 3);

  // The one-shot VACUUM ran, succeeded, and left no free pages behind.
  histograms.ExpectUniqueSample(
      "CriticalActions.Database.MigrationVacuumResult", true, 1);
  sql::Statement s(db.GetReadonlyStatement("PRAGMA freelist_count"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(0, s.ColumnInt(0));
}

TEST_F(CriticalActionDatabaseTest, FreshDatabaseDoesNotVacuum) {
  base::HistogramTester histograms;
  {
    CriticalActionDatabase database(db_path_);
    ASSERT_TRUE(database.Init());
    histograms.ExpectTotalCount(
        "CriticalActions.Database.MigrationVacuumResult", 0);
  }
  {
    CriticalActionDatabase database(db_path_);
    ASSERT_TRUE(database.Init());
    histograms.ExpectTotalCount(
        "CriticalActions.Database.MigrationVacuumResult", 0);
  }
}

TEST_F(CriticalActionDatabaseTest, SchemaHasNoUrlColumn) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());
  auto& db = database.GetDBForTesting();
  EXPECT_FALSE(db.DoesColumnExist("CriticalActionEntries", "url"));
  EXPECT_FALSE(db.DoesIndexExist("idx_entries_url"));
  static constexpr base::cstring_view kExpectedColumns[] = {
      "critical_action_id", "timestamp", "actor_task_id", "action_type",
      "metadata"};
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
  std::string query =
      CriticalActionDatabase::BuildGetCriticalActionsQuery(options);
  EXPECT_THAT(query, HasSubstr("SELECT e.critical_action_id"));
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
  std::string query =
      CriticalActionDatabase::BuildGetCriticalActionsQuery(options);
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

  std::string query =
      CriticalActionDatabase::BuildGetCriticalActionsQuery(options);
  EXPECT_THAT(query, HasSubstr("WHERE e.action_type IN (?,?) AND "
                               "v.visit_id IN (?,?,?) AND "
                               "c.conversation_id = ? AND "
                               "e.actor_task_id = ?"));
  EXPECT_THAT(query, HasSubstr("ORDER BY e.timestamp DESC LIMIT ?"));
}

namespace {

struct SetCriticalActionsConversationIdTestCase {
  std::string test_name;
  std::vector<std::string> actor_task_ids_to_resolve;
  std::vector<std::string> entry_task_ids;
  base::flat_map<std::string, std::string> expected_conversation_ids;
};

}  // namespace

class SetCriticalActionsConversationIdTest
    : public CriticalActionDatabaseTest,
      public ::testing::WithParamInterface<
          SetCriticalActionsConversationIdTestCase> {};

// Tests SetCriticalActionsConversationId with various task ID sets.
TEST_P(SetCriticalActionsConversationIdTest, SetCriticalActionsConversationId) {
  const SetCriticalActionsConversationIdTestCase& test_case = GetParam();

  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  std::vector<CriticalActionEntry> entries;
  for (const auto& task_id : test_case.entry_task_ids) {
    CriticalActionEntry entry = CreateDefaultEntry();
    entry.actor_task_id = task_id;
    EXPECT_TRUE(database.AddCriticalAction(entry));
    entries.push_back(entry);
  }

  // Before resolution, none of the entries should have a conversation ID.
  for (const auto& entry : entries) {
    SCOPED_TRACE(entry.actor_task_id);
    auto retrieved = database.GetCriticalAction(entry.critical_action_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_TRUE(retrieved->conversation_id.empty());
  }

  EXPECT_TRUE(database.SetCriticalActionsConversationId(
      test_case.actor_task_ids_to_resolve, "conv_resolved1"));

  for (const auto& entry : entries) {
    SCOPED_TRACE(entry.actor_task_id);
    auto retrieved = database.GetCriticalAction(entry.critical_action_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->conversation_id,
              test_case.expected_conversation_ids.at(entry.actor_task_id));
  }

  database.Close();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SetCriticalActionsConversationIdTest,
    testing::Values(
        SetCriticalActionsConversationIdTestCase{
            .test_name = "SubsetResolves",
            .actor_task_ids_to_resolve = {"task_1", "task_2"},
            .entry_task_ids = {"task_1", "task_2", "task_other"},
            .expected_conversation_ids = {{"task_1", "conv_resolved1"},
                                          {"task_2", "conv_resolved1"},
                                          {"task_other", ""}},
        },
        SetCriticalActionsConversationIdTestCase{
            .test_name = "AllResolve",
            .actor_task_ids_to_resolve = {"task_1", "task_2", "task_other"},
            .entry_task_ids = {"task_1", "task_2", "task_other"},
            .expected_conversation_ids = {{"task_1", "conv_resolved1"},
                                          {"task_2", "conv_resolved1"},
                                          {"task_other", "conv_resolved1"}},
        },
        SetCriticalActionsConversationIdTestCase{
            .test_name = "NoneResolve",
            .actor_task_ids_to_resolve = {"task_unknown"},
            .entry_task_ids = {"task_1", "task_2", "task_other"},
            .expected_conversation_ids = {{"task_1", ""},
                                          {"task_2", ""},
                                          {"task_other", ""}},
        }),
    [](const testing::TestParamInfo<SetCriticalActionsConversationIdTestCase>&
           info) { return info.param.test_name; });

TEST_F(CriticalActionDatabaseTest,
       SetCriticalActionsConversationId_OnlyUpdatesMostRecentAction) {
  CriticalActionDatabase database(db_path_);
  ASSERT_TRUE(database.Init());

  base::Time base_time = base::Time::Now();
  const std::string kActorTaskId = "test_task";
  const std::string kConversationId = "test_conv";

  CriticalActionEntry older_entry = CreateDefaultEntry();
  older_entry.actor_task_id = kActorTaskId;
  older_entry.timestamp = base_time - base::Hours(1);
  ASSERT_TRUE(database.AddCriticalAction(older_entry));

  CriticalActionEntry newer_entry = CreateDefaultEntry();
  newer_entry.actor_task_id = kActorTaskId;
  newer_entry.timestamp = base_time;
  ASSERT_TRUE(database.AddCriticalAction(newer_entry));

  EXPECT_TRUE(database.SetCriticalActionsConversationId({kActorTaskId},
                                                        kConversationId));

  // Only the most recent critical action for `kActorTaskId` should be updated.
  auto retrieved_newer =
      database.GetCriticalAction(newer_entry.critical_action_id);
  ASSERT_TRUE(retrieved_newer.has_value());
  EXPECT_EQ(retrieved_newer->conversation_id, kConversationId);

  auto retrieved_older =
      database.GetCriticalAction(older_entry.critical_action_id);
  ASSERT_TRUE(retrieved_older.has_value());
  EXPECT_TRUE(retrieved_older->conversation_id.empty());

  database.Close();
}

}  // namespace critical_actions
