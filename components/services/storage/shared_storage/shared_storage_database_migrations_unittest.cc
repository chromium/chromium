// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_database_migrations.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "components/services/storage/shared_storage/shared_storage_test_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace storage {

namespace {

std::string RemoveQuotes(std::string input) {
  std::string output;
  base::RemoveChars(input, "\"", &output);
  return output;
}

}  // namespace

class SharedStorageDatabaseMigrationsTest : public testing::Test {
 public:
  SharedStorageDatabaseMigrationsTest() {
    special_storage_policy_ = base::MakeRefCounted<MockSpecialStoragePolicy>();
  }

  ~SharedStorageDatabaseMigrationsTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageInitTries", "2"}});

    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_name_ = temp_dir_.GetPath().AppendASCII("TestSharedStorage.db");
  }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  void MigrateDatabase() {
    auto shared_storage_db = std::make_unique<SharedStorageDatabase>(
        file_name_, special_storage_policy_,
        SharedStorageOptions::Create()->GetDatabaseOptions());

    // We need to run an operation on storage to force the lazy initialization.
    std::ignore = shared_storage_db->Get(
        url::Origin::Create(GURL("http://google.com/")), u"key1");
  }

  std::string GetCurrentSchema() {
    base::FilePath current_version_path =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("TestCurrentVersion.db"));
    EXPECT_TRUE(CreateDatabaseFromSQL(current_version_path,
                                      GetTestFileNameForCurrentVersion()));
    sql::Database db;
    EXPECT_TRUE(db.Open(current_version_path));
    return db.GetSchema();
  }

  int VersionFromDatabase(sql::Database& db) {
    // Get version.
    sql::Statement statement(
        db.GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
    if (!statement.Step())
      return 0;
    return statement.ColumnInt(0);
  }

  int CompatibleVersionFromDatabase(sql::Database& db) {
    // Get compatible version.
    sql::Statement statement(db.GetUniqueStatement(
        "SELECT value FROM meta WHERE key='last_compatible_version'"));
    if (!statement.Step())
      return 0;
    return statement.ColumnInt(0);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
};

TEST_F(SharedStorageDatabaseMigrationsTest, MigrateEmptyToCurrent) {
  {
    auto shared_storage_db = std::make_unique<SharedStorageDatabase>(
        file_name_, special_storage_policy_,
        SharedStorageOptions::Create()->GetDatabaseOptions());

    // We need to run a non-trivial operation on storage to force the lazy
    // initialization.
    std::ignore = shared_storage_db->Set(
        url::Origin::Create(GURL("http://google.com/")), u"key0", u"value0");
  }

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // Check version.
    EXPECT_EQ(SharedStorageDatabase::kCurrentVersionNumber,
              VersionFromDatabase(db));

    // Check that expected tables are present.
    EXPECT_TRUE(db.DoesTableExist("meta"));
    EXPECT_TRUE(db.DoesTableExist("values_mapping"));
    EXPECT_TRUE(db.DoesTableExist("per_origin_mapping"));
    EXPECT_TRUE(db.DoesTableExist("budget_mapping"));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));
  }
}

TEST_F(SharedStorageDatabaseMigrationsTest,
       MigrateLastDeprecatedVersionToCurrent) {
  ASSERT_TRUE(CreateDatabaseFromSQL(
      file_name_, GetTestFileNameForLatestDeprecatedVersion()));

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // Check version.
    EXPECT_EQ(SharedStorageDatabase::kDeprecatedVersionNumber,
              VersionFromDatabase(db));

    sql::Statement statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM values_mapping"));

    ASSERT_TRUE(statement.Step());
    ASSERT_LT(0, statement.ColumnInt(0));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // Check version.
    EXPECT_EQ(SharedStorageDatabase::kCurrentVersionNumber,
              VersionFromDatabase(db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is not preserved across the migration.
    sql::Statement statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM values_mapping"));

    ASSERT_TRUE(statement.Step());
    ASSERT_EQ(0, statement.ColumnInt(0));
  }
}

TEST_F(SharedStorageDatabaseMigrationsTest, MigrateTooNewVersionToCurrent) {
  ASSERT_TRUE(
      CreateDatabaseFromSQL(file_name_, "shared_storage.init_too_new.sql"));

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // Check compatible version.
    EXPECT_LT(SharedStorageDatabase::kCurrentVersionNumber,
              CompatibleVersionFromDatabase(db));

    sql::Statement statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM values_mapping"));

    ASSERT_TRUE(statement.Step());
    ASSERT_LT(0, statement.ColumnInt(0));
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // Check version.
    EXPECT_EQ(SharedStorageDatabase::kCurrentVersionNumber,
              VersionFromDatabase(db));

    // Check compatible version.
    EXPECT_GE(SharedStorageDatabase::kCurrentVersionNumber,
              CompatibleVersionFromDatabase(db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is not preserved across the migration.
    sql::Statement statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM values_mapping"));

    ASSERT_TRUE(statement.Step());
    ASSERT_EQ(0, statement.ColumnInt(0));
  }
}

TEST_F(SharedStorageDatabaseMigrationsTest, MigrateVersion1ToCurrent) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, GetTestFileNameForVersion(1)));

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // `context_origin`, `key`, and `value`.
    EXPECT_EQ(3u, sql::test::CountTableColumns(&db, "values_mapping"));

    // Implicit index on `meta`, `per_origin_mapping_last_used_time_idx`,
    // and budget_mapping_origin_time_stamp_idx.
    EXPECT_EQ(3u, sql::test::CountSQLIndices(&db));

    ASSERT_FALSE(db.DoesColumnExist("values_mapping", "last_used_time"));
    ASSERT_TRUE(db.DoesColumnExist("per_origin_mapping", "last_used_time"));
    ASSERT_FALSE(db.DoesColumnExist("per_origin_mapping", "creation_time"));
    ASSERT_FALSE(db.DoesIndexExist("values_mapping_last_used_time_idx"));
    ASSERT_TRUE(db.DoesIndexExist("per_origin_mapping_last_used_time_idx"));
    ASSERT_FALSE(db.DoesIndexExist("per_origin_mapping_creation_time_idx"));
  }

  MigrateDatabase();
  base::Time now = base::Time::Now();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // Check version.
    EXPECT_EQ(SharedStorageDatabase::kCurrentVersionNumber,
              VersionFromDatabase(db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement count_statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM values_mapping"));

    ASSERT_TRUE(count_statement.Step());
    ASSERT_LT(0, count_statement.ColumnInt(0));

    // Verify that the `last_used_time` in `values_mapping` is the time recorded
    // as the current time just after migration (within a tolerance).
    sql::Statement select_statement(
        db.GetUniqueStatement("SELECT * FROM values_mapping"));

    ASSERT_TRUE(select_statement.Step());
    base::Time last_used_time = select_statement.ColumnTime(3);
    ASSERT_LE(last_used_time, now);
    ASSERT_GE(last_used_time, now - TestTimeouts::action_max_timeout());
  }
}

// Test loading version 1 database with no budget tables.
TEST_F(SharedStorageDatabaseMigrationsTest,
       MigrateVersion1NoBudgetTablesToCurrent) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_,
                                    "shared_storage.v1.no_budget_table.sql"));

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // `meta`, `values_mapping`, and `per_origin_mapping`.
    EXPECT_EQ(3u, sql::test::CountSQLTables(&db));

    // `context_origin`, `key`, and `value`.
    EXPECT_EQ(3u, sql::test::CountTableColumns(&db, "values_mapping"));

    // Implicit index on `meta`, and `per_origin_mapping_last_used_time_idx`.
    EXPECT_EQ(2u, sql::test::CountSQLIndices(&db));

    ASSERT_FALSE(db.DoesTableExist("budget_mapping"));
    ASSERT_FALSE(db.DoesColumnExist("values_mapping", "last_used_time"));
    ASSERT_TRUE(db.DoesColumnExist("per_origin_mapping", "last_used_time"));
    ASSERT_FALSE(db.DoesColumnExist("per_origin_mapping", "creation_time"));
    ASSERT_FALSE(db.DoesIndexExist("values_mapping_last_used_time_idx"));
    ASSERT_TRUE(db.DoesIndexExist("per_origin_mapping_last_used_time_idx"));
    ASSERT_FALSE(db.DoesIndexExist("per_origin_mapping_creation_time_idx"));
    ASSERT_FALSE(db.DoesIndexExist("budget_mapping_origin_time_stamp_idx"));
  }

  MigrateDatabase();
  base::Time now = base::Time::Now();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // Check version.
    EXPECT_EQ(SharedStorageDatabase::kCurrentVersionNumber,
              VersionFromDatabase(db));

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(GetCurrentSchema()), RemoveQuotes(db.GetSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement count_statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM values_mapping"));

    ASSERT_TRUE(count_statement.Step());
    ASSERT_LT(0, count_statement.ColumnInt(0));

    // Verify that the `last_used_time` in `values_mapping` is the time recorded
    // as the current time just after migration (within a tolerance).
    sql::Statement select_statement(
        db.GetUniqueStatement("SELECT * FROM values_mapping"));

    ASSERT_TRUE(select_statement.Step());
    base::Time last_used_time = select_statement.ColumnTime(3);
    ASSERT_LE(last_used_time, now);
    ASSERT_GE(last_used_time, now - TestTimeouts::action_max_timeout());
  }
}

}  // namespace storage
