// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_database_migrations.h"

#include <memory>
#include <tuple>
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

TEST_F(SharedStorageDatabaseMigrationsTest, MigrateVersion5ToCurrent) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, GetTestFileNameForVersion(5)));
  std::map<std::string, std::tuple<base::Time, int64_t, int64_t>>
      premigration_values;

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // `context_origin`, `creation_time`, `length`, and `num_bytes`.
    EXPECT_EQ(4u, sql::test::CountTableColumns(&db, "per_origin_mapping"));

    // Implicit index on `meta`, `values_mapping_last_used_time_idx`,
    // `per_origin_mapping_creation_time_idx`, and
    // budget_mapping_site_time_stamp_idx.
    EXPECT_EQ(4u, sql::test::CountSQLIndices(&db));

    ASSERT_TRUE(db.DoesColumnExist("per_origin_mapping", "length"));

    sql::Statement select_origins_statement(
        db.GetUniqueStatement("SELECT * FROM per_origin_mapping"));

    while (select_origins_statement.Step()) {
      premigration_values[select_origins_statement.ColumnString(0)] =
          std::make_tuple(select_origins_statement.ColumnTime(1),
                          select_origins_statement.ColumnInt64(2),
                          select_origins_statement.ColumnInt64(3));
    }

    ASSERT_TRUE(select_origins_statement.Succeeded());
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

    // `context_origin`, `creation_time`, and `num_bytes`.
    EXPECT_EQ(3u, sql::test::CountTableColumns(&db, "per_origin_mapping"));

    // Implicit index on `meta`, `values_mapping_last_used_time_idx`,
    // `per_origin_mapping_creation_time_idx`, and
    // budget_mapping_site_time_stamp_idx.
    EXPECT_EQ(4u, sql::test::CountSQLIndices(&db));

    ASSERT_FALSE(db.DoesColumnExist("per_origin_mapping", "length"));

    // Verify that there is data preserved across the migration.
    sql::Statement count_statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM budget_mapping"));

    ASSERT_TRUE(count_statement.Step());
    ASSERT_LT(0, count_statement.ColumnInt(0));

    sql::Statement select_statement(
        db.GetUniqueStatement("SELECT * FROM per_origin_mapping"));

    // Verify that the premigration values are preserved in the preexisting
    // columns of `per_origin_mapping`, except for the `length` column.
    while (select_statement.Step()) {
      std::string origin = select_statement.ColumnString(0);
      auto id_it = premigration_values.find(origin);
      ASSERT_TRUE(id_it != premigration_values.end());

      EXPECT_EQ(std::get<0>(id_it->second), select_statement.ColumnTime(1));
      EXPECT_EQ(std::get<2>(id_it->second), select_statement.ColumnInt64(2));
    }
  }
}

TEST_F(SharedStorageDatabaseMigrationsTest, MigrateVersion4ToCurrent) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, GetTestFileNameForVersion(4)));
  std::map<std::string, std::tuple<base::Time, int64_t>> premigration_values;
  std::map<std::string, int64_t> num_bytes_map;

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // `context_origin`, `creation_time`, and `length`.
    EXPECT_EQ(3u, sql::test::CountTableColumns(&db, "per_origin_mapping"));

    // Implicit index on `meta`, `values_mapping_last_used_time_idx`,
    // `per_origin_mapping_creation_time_idx`, and
    // budget_mapping_site_time_stamp_idx.
    EXPECT_EQ(4u, sql::test::CountSQLIndices(&db));

    ASSERT_TRUE(db.DoesColumnExist("per_origin_mapping", "length"));
    ASSERT_FALSE(db.DoesColumnExist("per_origin_mapping", "num_bytes"));

    sql::Statement select_origins_statement(
        db.GetUniqueStatement("SELECT * FROM per_origin_mapping"));

    while (select_origins_statement.Step()) {
      premigration_values[select_origins_statement.ColumnString(0)] =
          std::make_tuple(select_origins_statement.ColumnTime(1),
                          select_origins_statement.ColumnInt64(2));
    }

    ASSERT_TRUE(select_origins_statement.Succeeded());

    sql::Statement select_values_statement(db.GetUniqueStatement(
        "SELECT context_origin, key, value FROM values_mapping"));

    while (select_values_statement.Step()) {
      std::u16string key;
      ASSERT_TRUE(select_values_statement.ColumnBlobAsString16(1, &key));
      std::u16string value;
      ASSERT_TRUE(select_values_statement.ColumnBlobAsString16(2, &value));
      int64_t bytes_delta = 2 * (key.size() + value.size());
      std::string origin = select_values_statement.ColumnString(0);
      auto it = num_bytes_map.find(origin);
      if (it != num_bytes_map.end()) {
        it->second += bytes_delta;
      } else {
        num_bytes_map[origin] = bytes_delta;
      }
    }

    ASSERT_TRUE(select_values_statement.Succeeded());
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

    // `context_origin`, `creation_time`, and `num_bytes`.
    EXPECT_EQ(3u, sql::test::CountTableColumns(&db, "per_origin_mapping"));

    // Implicit index on `meta`, `values_mapping_last_used_time_idx`,
    // `per_origin_mapping_creation_time_idx`, and
    // budget_mapping_site_time_stamp_idx.
    EXPECT_EQ(4u, sql::test::CountSQLIndices(&db));

    ASSERT_FALSE(db.DoesColumnExist("per_origin_mapping", "length"));
    ASSERT_TRUE(db.DoesColumnExist("per_origin_mapping", "num_bytes"));

    // Verify that there is data preserved across the migration.
    sql::Statement count_statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM budget_mapping"));

    ASSERT_TRUE(count_statement.Step());
    ASSERT_LT(0, count_statement.ColumnInt(0));

    sql::Statement select_statement(
        db.GetUniqueStatement("SELECT * FROM per_origin_mapping"));

    // Verify that the premigration values are preserved in the preexisting
    // columns of `per_origin_mapping`, except for the `length` column, and that
    // `num_bytes` is the total number of bytes stored in the columns `key` and
    // `value` of `values_mapping` for `context_origin`.
    while (select_statement.Step()) {
      std::string origin = select_statement.ColumnString(0);
      auto id_it = premigration_values.find(origin);
      ASSERT_TRUE(id_it != premigration_values.end());

      EXPECT_EQ(std::get<0>(id_it->second), select_statement.ColumnTime(1));

      auto by_it = num_bytes_map.find(origin);
      ASSERT_TRUE(by_it != num_bytes_map.end());
      EXPECT_EQ(by_it->second, select_statement.ColumnInt64(2));
    }
  }
}

TEST_F(SharedStorageDatabaseMigrationsTest, MigrateVersion3ToCurrent) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, GetTestFileNameForVersion(3)));
  std::map<int64_t, std::tuple<url::Origin, base::Time, double>>
      premigration_values;

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // `id`, `context_origin`, `time_stamp`, and `bits_debit`.
    EXPECT_EQ(4u, sql::test::CountTableColumns(&db, "budget_mapping"));

    // Implicit index on `meta`, `per_origin_mapping_last_used_time_idx`,
    // `budget_mapping_origin_time_stamp_idx`, and
    // `values_mapping_last_used_time_idx`.
    EXPECT_EQ(4u, sql::test::CountSQLIndices(&db));

    ASSERT_TRUE(db.DoesColumnExist("budget_mapping", "context_origin"));
    ASSERT_FALSE(db.DoesColumnExist("budget_mapping", "context_site"));

    sql::Statement select_statement(
        db.GetUniqueStatement("SELECT * FROM budget_mapping"));

    while (select_statement.Step()) {
      premigration_values[select_statement.ColumnInt64(0)] = std::make_tuple(
          url::Origin::Create(GURL(select_statement.ColumnString(1))),
          select_statement.ColumnTime(2), select_statement.ColumnDouble(3));
    }
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

    ASSERT_TRUE(db.DoesColumnExist("budget_mapping", "context_site"));
    ASSERT_FALSE(db.DoesColumnExist("budget_mapping", "context_origin"));

    // Verify that data is preserved across the migration.
    sql::Statement count_statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM budget_mapping"));

    ASSERT_TRUE(count_statement.Step());
    ASSERT_LT(0, count_statement.ColumnInt(0));

    // Verify that each `context_site` in `budget_mapping` is the site for the
    // previously stored `context_origin`.
    sql::Statement select_statement(
        db.GetUniqueStatement("SELECT * FROM budget_mapping"));

    while (select_statement.Step()) {
      auto id_it = premigration_values.find(select_statement.ColumnInt64(0));
      ASSERT_TRUE(id_it != premigration_values.end());

      EXPECT_EQ(
          net::SchemefulSite::Deserialize(select_statement.ColumnString(1)),
          net::SchemefulSite(std::get<0>(id_it->second)));
      EXPECT_EQ(std::get<1>(id_it->second), select_statement.ColumnTime(2));
      EXPECT_EQ(std::get<2>(id_it->second), select_statement.ColumnDouble(3));
    }
  }
}

TEST_F(SharedStorageDatabaseMigrationsTest, MigrateVersion2ToCurrent) {
  ASSERT_TRUE(CreateDatabaseFromSQL(file_name_, GetTestFileNameForVersion(2)));
  std::map<std::string,
           std::map<std::u16string, std::pair<std::u16string, base::Time>>>
      premigration_values;

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(file_name_));

    // `context_origin`, `key`, `value`, and `last_used_time`.
    EXPECT_EQ(4u, sql::test::CountTableColumns(&db, "values_mapping"));

    // Implicit index on `meta`, `per_origin_mapping_last_used_time_idx`,
    // `budget_mapping_origin_time_stamp_idx`, and
    // `values_mapping_last_used_time_idx`.
    EXPECT_EQ(4u, sql::test::CountSQLIndices(&db));

    ASSERT_TRUE(db.DoesColumnExist("values_mapping", "key"));
    ASSERT_TRUE(db.DoesColumnExist("values_mapping", "value"));

    sql::test::ColumnInfo key_column_info =
        sql::test::ColumnInfo::Create(&db, "main", "values_mapping", "key");
    EXPECT_EQ("TEXT", key_column_info.data_type);
    sql::test::ColumnInfo value_column_info =
        sql::test::ColumnInfo::Create(&db, "main", "values_mapping", "key");
    EXPECT_EQ("TEXT", value_column_info.data_type);

    sql::Statement select_statement(
        db.GetUniqueStatement("SELECT * FROM values_mapping"));

    while (select_statement.Step()) {
      premigration_values[select_statement.ColumnString(0)]
                         [select_statement.ColumnString16(1)] =
                             std::make_pair(select_statement.ColumnString16(2),
                                            select_statement.ColumnTime(3));
    }
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

    ASSERT_TRUE(db.DoesColumnExist("values_mapping", "key"));
    ASSERT_TRUE(db.DoesColumnExist("values_mapping", "value"));

    sql::test::ColumnInfo key_column_info =
        sql::test::ColumnInfo::Create(&db, "main", "values_mapping", "key");
    EXPECT_EQ("BLOB", key_column_info.data_type);
    sql::test::ColumnInfo value_column_info =
        sql::test::ColumnInfo::Create(&db, "main", "values_mapping", "key");
    EXPECT_EQ("BLOB", value_column_info.data_type);

    // Verify that data is preserved across the migration.
    sql::Statement count_statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM values_mapping"));

    ASSERT_TRUE(count_statement.Step());
    ASSERT_LT(0, count_statement.ColumnInt(0));

    // Verify that the `key` and `value` in `values_mapping` are the UTF-16 hex
    // bytes of the pre-migration `key` and `value`.
    sql::Statement select_statement(
        db.GetUniqueStatement("SELECT * FROM values_mapping"));

    while (select_statement.Step()) {
      auto origin_it =
          premigration_values.find(select_statement.ColumnString(0));
      ASSERT_TRUE(origin_it != premigration_values.end());
      std::u16string key;
      ASSERT_TRUE(select_statement.ColumnBlobAsString16(1, &key));
      auto key_it = origin_it->second.find(key);
      ASSERT_TRUE(key_it != origin_it->second.end());
      std::u16string value;
      ASSERT_TRUE(select_statement.ColumnBlobAsString16(2, &value));
      EXPECT_EQ(key_it->second.first, value);
      EXPECT_EQ(key_it->second.second, select_statement.ColumnTime(3));
    }
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
