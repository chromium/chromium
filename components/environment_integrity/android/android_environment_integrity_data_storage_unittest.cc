// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/environment_integrity/android/android_environment_integrity_data_storage.h"

#include <functional>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace environment_integrity {

namespace {

int VersionFromMetaTable(sql::Database& db) {
  // Get version.
  sql::Statement s(
      db.GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
  if (!s.Step()) {
    return 0;
  }
  return s.ColumnInt(0);
}

}  // namespace

class AndroidEnvironmentIntegrityDataStorageTest : public testing::Test {
 public:
  AndroidEnvironmentIntegrityDataStorageTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  base::FilePath DbPath() const {
    return temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("EnvironmentIntegrity"));
  }

  base::FilePath GetSqlFilePath(base::StringPiece sql_filename) {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path =
        file_path.AppendASCII("components/test/data/environment_integrity/");
    file_path = file_path.AppendASCII(sql_filename);
    EXPECT_TRUE(base::PathExists(file_path));
    return file_path;
  }

  size_t CountHandleEntries(sql::Database& db) {
    static const char kCountSQL[] =
        "SELECT COUNT(*) FROM environment_integrity_handles";
    sql::Statement s(db.GetUniqueStatement(kCountSQL));
    EXPECT_TRUE(s.Step());
    return s.ColumnInt(0);
  }

  void OpenDatabase() {
    storage_.reset();
    storage_ =
        std::make_unique<AndroidEnvironmentIntegrityDataStorage>(DbPath());
  }

  void CloseDatabase() { storage_.reset(); }

  AndroidEnvironmentIntegrityDataStorage* storage() { return storage_.get(); }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AndroidEnvironmentIntegrityDataStorage> storage_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AndroidEnvironmentIntegrityDataStorageTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  OpenDatabase();
  CloseDatabase();

  // An unused AndroidEnvironmentIntegrityDataStorage instance should not create
  // the database.
  EXPECT_FALSE(base::PathExists(DbPath()));

  OpenDatabase();
  // Trigger the lazy-initialization.
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  storage()->GetHandle(origin);
  CloseDatabase();

  EXPECT_TRUE(base::PathExists(DbPath()));

  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));

  // [environment_integrity_handles], [meta].
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));

  EXPECT_EQ(1, VersionFromMetaTable(db));

  // [sqlite_autoindex_environment_integrity_handles_1] and
  // [sqlite_autoindex_meta_1].
  EXPECT_EQ(2u, sql::test::CountSQLIndices(&db));

  // `origin` and `handle`.
  EXPECT_EQ(2u,
            sql::test::CountTableColumns(&db, "environment_integrity_handles"));

  EXPECT_EQ(0u, CountHandleEntries(db));
}

TEST_F(AndroidEnvironmentIntegrityDataStorageTest,
       LoadFromFile_CurrentVersion_Success) {
  ASSERT_TRUE(
      sql::test::CreateDatabaseFromSQL(DbPath(), GetSqlFilePath("v1.sql")));

  OpenDatabase();
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  // Trigger the lazy-initialization.
  absl::optional<int64_t> maybe_handle = storage()->GetHandle(origin);
  ASSERT_TRUE(maybe_handle.has_value());
  EXPECT_EQ(*maybe_handle, 123);
  CloseDatabase();

  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(1, VersionFromMetaTable(db));
  EXPECT_EQ(1u, CountHandleEntries(db));
}

TEST_F(AndroidEnvironmentIntegrityDataStorageTest,
       LoadFromFile_VersionTooOld_Failure) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      DbPath(), GetSqlFilePath("v0.init_too_old.sql")));

  OpenDatabase();
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  // Trigger the lazy-initialization.
  absl::optional<int64_t> maybe_handle = storage()->GetHandle(origin);
  EXPECT_FALSE(maybe_handle);
  CloseDatabase();

  // Expect that the initialization was unsuccessful. The original database was
  // unaffected.
  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(0, VersionFromMetaTable(db));
  EXPECT_EQ(1u, CountHandleEntries(db));
}

TEST_F(AndroidEnvironmentIntegrityDataStorageTest,
       LoadFromFile_VersionTooNew_Failure) {
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(
      DbPath(), GetSqlFilePath("v2.init_too_new.sql")));

  OpenDatabase();
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  // Trigger the lazy-initialization.
  absl::optional<int64_t> maybe_handle = storage()->GetHandle(origin);
  EXPECT_FALSE(maybe_handle);
  CloseDatabase();

  // Expect that the initialization was successful. The original database was
  // razed and re-initialized.
  sql::Database db;
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(2u, sql::test::CountSQLTables(&db));
  EXPECT_EQ(1, VersionFromMetaTable(db));
  EXPECT_EQ(0u, CountHandleEntries(db));
}

TEST_F(AndroidEnvironmentIntegrityDataStorageTest, GetAndSetHandle) {
  OpenDatabase();
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));
  absl::optional<int64_t> maybe_handle = storage()->GetHandle(origin);
  EXPECT_FALSE(maybe_handle);

  int64_t handle = 123;
  storage()->SetHandle(origin, handle);
  maybe_handle = storage()->GetHandle(origin);
  ASSERT_TRUE(maybe_handle.has_value());
  EXPECT_EQ(*maybe_handle, handle);

  int64_t new_handle = 234;
  storage()->SetHandle(origin, new_handle);
  maybe_handle = storage()->GetHandle(origin);
  ASSERT_TRUE(maybe_handle.has_value());
  EXPECT_EQ(*maybe_handle, new_handle);
  CloseDatabase();
}

TEST_F(AndroidEnvironmentIntegrityDataStorageTest, ClearDataForOrigin) {
  OpenDatabase();
  url::Origin origin1 = url::Origin::Create(GURL("https://foo.com"));
  int64_t handle1 = 123;
  url::Origin origin2 = url::Origin::Create(GURL("https://bar.com"));
  int64_t handle2 = 234;

  storage()->SetHandle(origin1, handle1);
  storage()->SetHandle(origin2, handle2);

  absl::optional<int64_t> maybe_handle1 = storage()->GetHandle(origin1);
  absl::optional<int64_t> maybe_handle2 = storage()->GetHandle(origin2);
  EXPECT_TRUE(maybe_handle1.has_value());
  EXPECT_EQ(maybe_handle1.value(), handle1);
  EXPECT_TRUE(maybe_handle2.has_value());
  EXPECT_EQ(maybe_handle2.value(), handle2);

  storage()->ClearData(
      base::BindRepeating(std::equal_to<blink::StorageKey>(),
                          blink::StorageKey::CreateFirstParty(origin1)));

  maybe_handle1 = storage()->GetHandle(origin1);
  maybe_handle2 = storage()->GetHandle(origin2);
  EXPECT_FALSE(maybe_handle1.has_value());
  EXPECT_TRUE(maybe_handle2.has_value());
  EXPECT_EQ(maybe_handle2.value(), handle2);
  CloseDatabase();
}

TEST_F(AndroidEnvironmentIntegrityDataStorageTest, ClearAllData) {
  OpenDatabase();
  url::Origin origin1 = url::Origin::Create(GURL("https://foo.com"));
  int64_t handle1 = 123;
  url::Origin origin2 = url::Origin::Create(GURL("https://bar.com"));
  int64_t handle2 = 234;

  storage()->SetHandle(origin1, handle1);
  storage()->SetHandle(origin2, handle2);

  absl::optional<int64_t> maybe_handle1 = storage()->GetHandle(origin1);
  absl::optional<int64_t> maybe_handle2 = storage()->GetHandle(origin2);
  EXPECT_TRUE(maybe_handle1.has_value());
  EXPECT_EQ(maybe_handle1.value(), handle1);
  EXPECT_TRUE(maybe_handle2.has_value());
  EXPECT_EQ(maybe_handle2.value(), handle2);

  url::Origin opaque_origin;
  storage()->ClearData(base::NullCallback());

  maybe_handle1 = storage()->GetHandle(origin1);
  maybe_handle2 = storage()->GetHandle(origin2);
  EXPECT_FALSE(maybe_handle1.has_value());
  EXPECT_FALSE(maybe_handle2.has_value());
  CloseDatabase();
}

}  // namespace environment_integrity
