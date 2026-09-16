// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_encrypted_vfs/sqlite_encrypted_vfs.h"

#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace sqlite_encrypted_vfs {

class SqliteEncryptedVfsTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().AppendASCII("encrypted_vfs_test.sqlite");
    SqliteEncryptedVfs::Register();
  }

  sql::Database CreateDatabase() {
    return sql::Database(
        sql::DatabaseOptions()
            .set_vfs_name_discouraged(SqliteEncryptedVfs::kSqliteVfsName)
            .set_mmap_enabled(false),
        sql::test::kTestTag);
  }

  std::vector<base::FilePath> GetFilesInTempDir() const {
    std::vector<base::FilePath> files;
    base::FileEnumerator enumerator(temp_dir_.GetPath(), /*recursive=*/false,
                                    base::FileEnumerator::FILES);
    for (base::FilePath name = enumerator.Next(); !name.empty();
         name = enumerator.Next()) {
      files.push_back(name.BaseName());
    }
    return files;
  }

  void VerifyDatabaseFiles() const {
    EXPECT_TRUE(base::PathExists(db_path_));
    EXPECT_THAT(GetFilesInTempDir(),
                testing::UnorderedElementsAre(
                    db_path_.BaseName(),
                    sql::Database::JournalPath(db_path_).BaseName()));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
};

TEST_F(SqliteEncryptedVfsTest, GetInstance) {
  EXPECT_NE(SqliteEncryptedVfs::GetInstance(), nullptr);
}

TEST_F(SqliteEncryptedVfsTest, BasicOperations) {
  {
    sql::Database db = CreateDatabase();
    ASSERT_TRUE(db.Open(db_path_)) << db.GetErrorMessage();

    EXPECT_TRUE(
        db.Execute("CREATE TABLE foo (id INTEGER PRIMARY KEY, title TEXT)"))
        << db.GetErrorMessage();
    EXPECT_TRUE(
        db.Execute("INSERT INTO foo (id, title) VALUES (1, 'hello world')"))
        << db.GetErrorMessage();

    sql::Statement s(
        db.GetUniqueStatement("SELECT title FROM foo WHERE id = 1"));
    EXPECT_TRUE(s.is_valid()) << db.GetErrorMessage();
    EXPECT_TRUE(s.Step()) << db.GetErrorMessage();
    EXPECT_EQ(s.ColumnString(0), "hello world");
  }

  VerifyDatabaseFiles();
}

TEST_F(SqliteEncryptedVfsTest, TransactionAndRollback) {
  {
    sql::Database db = CreateDatabase();
    ASSERT_TRUE(db.Open(db_path_)) << db.GetErrorMessage();
    ASSERT_TRUE(
        db.Execute("CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT)"));

    // Rolled back transaction.
    {
      sql::Transaction transaction(&db);
      ASSERT_TRUE(transaction.Begin());
      ASSERT_TRUE(
          db.Execute("INSERT INTO items (id, name) VALUES (1, 'apple')"));
      transaction.Rollback();
    }

    {
      sql::Statement s(
          db.GetUniqueStatement("SELECT COUNT(*) FROM items WHERE id = 1"));
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(s.ColumnInt(0), 0);
    }

    // Committed transaction.
    {
      sql::Transaction transaction(&db);
      ASSERT_TRUE(transaction.Begin());
      ASSERT_TRUE(
          db.Execute("INSERT INTO items (id, name) VALUES (2, 'banana')"));
      ASSERT_TRUE(transaction.Commit());
    }

    {
      sql::Statement s(
          db.GetUniqueStatement("SELECT name FROM items WHERE id = 2"));
      ASSERT_TRUE(s.Step());
      EXPECT_EQ(s.ColumnString(0), "banana");
    }
  }

  VerifyDatabaseFiles();
}

TEST_F(SqliteEncryptedVfsTest, ReopenDatabase) {
  {
    sql::Database db = CreateDatabase();
    ASSERT_TRUE(db.Open(db_path_)) << db.GetErrorMessage();
    ASSERT_TRUE(db.Execute("CREATE TABLE kv (k TEXT PRIMARY KEY, v TEXT)"));
    ASSERT_TRUE(db.Execute("INSERT INTO kv (k, v) VALUES ('key1', 'val1')"));
    db.Close();
  }

  {
    sql::Database db = CreateDatabase();
    ASSERT_TRUE(db.Open(db_path_)) << db.GetErrorMessage();
    sql::Statement s(
        db.GetUniqueStatement("SELECT v FROM kv WHERE k = 'key1'"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(s.ColumnString(0), "val1");
  }

  VerifyDatabaseFiles();
}

TEST_F(SqliteEncryptedVfsTest, MmapFallbackToStandardIO) {
  {
    sql::Database db(
        sql::DatabaseOptions()
            .set_vfs_name_discouraged(SqliteEncryptedVfs::kSqliteVfsName)
            .set_mmap_enabled(true),
        sql::test::kTestTag);
    ASSERT_TRUE(db.Open(db_path_)) << db.GetErrorMessage();

    EXPECT_TRUE(
        db.Execute("CREATE TABLE data (id INTEGER PRIMARY KEY, content TEXT)"))
        << db.GetErrorMessage();
    EXPECT_TRUE(db.Execute(
        "INSERT INTO data (id, content) VALUES (1, 'fallback test')"))
        << db.GetErrorMessage();

    sql::Statement s(
        db.GetUniqueStatement("SELECT content FROM data WHERE id = 1"));
    ASSERT_TRUE(s.Step()) << db.GetErrorMessage();
    EXPECT_EQ(s.ColumnString(0), "fallback test");
  }

  VerifyDatabaseFiles();
}

TEST_F(SqliteEncryptedVfsTest, RegistrationIdempotencyAndMetadata) {
  // Repeated registration should not crash or duplicate.
  SqliteEncryptedVfs::Register();
  SqliteEncryptedVfs::Register();

  sqlite3_vfs* vfs = sqlite3_vfs_find(SqliteEncryptedVfs::kSqliteVfsName);
  ASSERT_NE(vfs, nullptr);
  EXPECT_STREQ(vfs->zName, SqliteEncryptedVfs::kSqliteVfsName);
  EXPECT_GE(vfs->iVersion, 3);
  EXPECT_GT(vfs->szOsFile, 0);
  EXPECT_EQ(vfs->xDlOpen, nullptr);  // Dynamic extensions disabled.
}

TEST_F(SqliteEncryptedVfsTest, MultiPageOperations) {
  {
    sql::Database db = CreateDatabase();
    ASSERT_TRUE(db.Open(db_path_)) << db.GetErrorMessage();
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE large (id INTEGER PRIMARY KEY, payload TEXT)"));

    std::string large_string(1024, 'x');
    sql::Statement s(
        db.GetUniqueStatement("INSERT INTO large (payload) VALUES (?)"));
    for (int i = 0; i < 50; ++i) {
      s.Reset(/*clear_bound_vars=*/true);
      s.BindString(0, large_string);
      ASSERT_TRUE(s.Run());
    }
  }

  {
    sql::Database db = CreateDatabase();
    ASSERT_TRUE(db.Open(db_path_)) << db.GetErrorMessage();
    sql::Statement count_stmt(
        db.GetUniqueStatement("SELECT COUNT(*) FROM large"));
    ASSERT_TRUE(count_stmt.Step());
    EXPECT_EQ(count_stmt.ColumnInt(0), 50);
  }

  VerifyDatabaseFiles();
}

}  // namespace sqlite_encrypted_vfs
