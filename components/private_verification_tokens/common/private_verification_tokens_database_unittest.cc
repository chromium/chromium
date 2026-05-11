// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_database.h"

#include <limits>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_verification_tokens {

namespace {

static constexpr base::FilePath::CharType kDatabaseFileName[] =
    FILE_PATH_LITERAL("PrivateVerificationTokens");
static constexpr char kKeyTableName[] = "keys";

class PrivateVerificationTokensDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().Append(kDatabaseFileName);
  }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  void VerifyTableRowCount(sql::Database& db, const char* table, size_t count) {
    size_t got_count = 0;
    ASSERT_TRUE(sql::test::CountTableRows(&db, table, &got_count));
    EXPECT_EQ(count, got_count);
  }

  void CreateDatabase(const base::FilePath& path) {
    pvt_database_.reset();
    pvt_database_ = PrivateVerificationTokensDatabase::Create(path);
    ASSERT_NE(pvt_database_, nullptr);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<PrivateVerificationTokensDatabase> pvt_database_;
};

TEST_F(PrivateVerificationTokensDatabaseTest, Create_ValidPath_Success) {
  std::unique_ptr<PrivateVerificationTokensDatabase> maybe_database =
      PrivateVerificationTokensDatabase::Create(db_path_);
  EXPECT_NE(maybe_database, nullptr);
}

TEST_F(PrivateVerificationTokensDatabaseTest, Create_EmptyPath_Failure) {
  const base::FilePath database_path;
  ASSERT_TRUE(database_path.empty());
  std::unique_ptr<PrivateVerificationTokensDatabase> maybe_database =
      PrivateVerificationTokensDatabase::Create(database_path);
  EXPECT_FALSE(base::PathExists(database_path));
  EXPECT_EQ(maybe_database, nullptr);
}

TEST_F(PrivateVerificationTokensDatabaseTest, Create_Unused_NoFileCreated) {
  CreateDatabase(db_path_);
  pvt_database_.reset();
  EXPECT_FALSE(base::PathExists(db_path_));
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       PathToDatabase_ValidPath_ReturnsPath) {
  CreateDatabase(db_path_);
  EXPECT_EQ(pvt_database_->PathToDatabase(), db_path_);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       StoreKeys_ValidKeys_StoredAndRetrieved) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  std::vector<uint8_t> key_a = {1, 2, 3};
  std::vector<uint8_t> key_b = {4, 5, 6};
  const auto exp = base::Time::FromTimeT(static_cast<time_t>(5));
  std::vector<PrivateVerificationTokensPublicKey> keys{
      PrivateVerificationTokensPublicKey("a.com", key_a, /*key_id=*/3,
                                         /*expiration=*/exp, /*version=*/1),
      PrivateVerificationTokensPublicKey("b.tri", key_b, /*key_id=*/4,
                                         /*expiration=*/exp, /*version=*/2),
  };
  EXPECT_TRUE(pvt_database_->StoreKeys(keys));

  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(6u, sql::test::CountTableColumns(&database, kKeyTableName));
  VerifyTableRowCount(database, kKeyTableName, 2u);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       InitializeDB_CorruptedFile_RazedAndReinitialized) {
  base::File corrupted_db_file(db_path_, base::File::FLAG_CREATE_ALWAYS |
                                             base::File::FLAG_WRITE |
                                             base::File::FLAG_READ);
  ASSERT_TRUE(corrupted_db_file.IsValid());
  constexpr char kCorruptedDatabase[] = "corrupted_database";
  corrupted_db_file.Write(0, base::as_byte_span(kCorruptedDatabase));
  corrupted_db_file.Close();

  // Expect that the corrupted database cannot be opened directly.
  sql::Database db(sql::test::kTestTag);
  EXPECT_FALSE(db.Open(db_path_));
  db.Close();

  // Create the Database with the corrupted database file.
  CreateDatabase(db_path_);

  // Trigger the lazy-initialization by attempting to store a key.
  std::vector<uint8_t> key_a = {1, 2, 3};
  const auto exp = base::Time::FromTimeT(static_cast<time_t>(5));
  std::vector<PrivateVerificationTokensPublicKey> keys{
      PrivateVerificationTokensPublicKey("a.com", key_a, /*key_id=*/3,
                                         /*expiration=*/exp, /*version=*/1),
  };

  EXPECT_TRUE(pvt_database_->StoreKeys(keys));
  pvt_database_.reset();

  // Expect that the corrupted database was razed and re-initialized and the new
  // database has one key.
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(6u, sql::test::CountTableColumns(&database, kKeyTableName));
  VerifyTableRowCount(database, kKeyTableName, 1u);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       InitializeDB_FailedToCreateDirectory) {
  base::FilePath file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("file_not_dir"));
  ASSERT_TRUE(base::WriteFile(file_path, "content"));

  base::FilePath invalid_db_path = file_path.Append(kDatabaseFileName);
  CreateDatabase(invalid_db_path);

  // Call StoreKeys with empty vector to trigger dbfile write.
  EXPECT_FALSE(pvt_database_->StoreKeys({}));

  EXPECT_FALSE(base::PathExists(invalid_db_path));
}

#if BUILDFLAG(IS_POSIX)
TEST_F(PrivateVerificationTokensDatabaseTest,
       InitializeDB_DirectoryNotWritable) {
  base::FilePath dir_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("readonly_dir"));
  ASSERT_TRUE(base::CreateDirectory(dir_path));
  ASSERT_TRUE(base::SetPosixFilePermissions(dir_path, 0555));

  base::FilePath db_path = dir_path.Append(kDatabaseFileName);
  CreateDatabase(db_path);

  // Call StoreKeys with empty vector to trigger dbfile write.
  EXPECT_FALSE(pvt_database_->StoreKeys({}));

  EXPECT_FALSE(base::PathExists(db_path));

  ASSERT_TRUE(base::SetPosixFilePermissions(dir_path, 0777));
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(PrivateVerificationTokensDatabaseTest,
       InitializeDB_FileAlreadyOpen_DcheckDeath) {
  EXPECT_FALSE(base::PathExists(db_path_));
  sql::Database db(sql::test::kTestTag);
  EXPECT_TRUE(db.Open(db_path_));

  CreateDatabase(db_path_);
  // Trigger the lazy-initialization
  EXPECT_DCHECK_DEATH_WITH(pvt_database_->StoreKeys({}),
                           "Unexpected Sqlite error");
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       LoadFromFile_VersionTooNew_RazedAndReinitialized) {
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(db_path_));
    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 2, 2));
  }

  CreateDatabase(db_path_);

  std::vector<uint8_t> key_a = {1, 2, 3};
  const auto exp = base::Time::FromTimeT(static_cast<time_t>(5));
  std::vector<PrivateVerificationTokensPublicKey> keys{
      PrivateVerificationTokensPublicKey("a.com", key_a, 3, exp, 1),
  };
  EXPECT_TRUE(pvt_database_->StoreKeys(keys));
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(6u, sql::test::CountTableColumns(&database, kKeyTableName));
  VerifyTableRowCount(database, kKeyTableName, 1u);
}

}  // namespace

}  // namespace private_verification_tokens
