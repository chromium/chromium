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
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace private_verification_tokens {

namespace {

static constexpr base::FilePath::CharType kDatabaseFileName[] =
    FILE_PATH_LITERAL("PrivateVerificationTokens");
static constexpr char kTokenTableName[] = "tokens";

// Get version column value from the given database meta table.
int VersionFromMetaTable(sql::Database& db) {
  sql::Statement s(
      db.GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
  if (!s.Step()) {
    NOTREACHED();
  }
  return s.ColumnInt(0);
}

// Returns a vector of tokens from a given map. Keys of the map argument are
// issuer origins.
std::vector<PrivateVerificationTokensToken> CreateTokens(
    std::map<url::Origin, std::vector<SerializedToken>> token_map,
    uint32_t key_id,
    const base::Time expiration,
    uint32_t version) {
  std::vector<PrivateVerificationTokensToken> result;
  for (const auto& site_tokens : token_map) {
    for (const auto& t : site_tokens.second) {
      result.emplace_back(site_tokens.first, t, key_id, expiration, version);
    }
  }
  return result;
}

class PrivateVerificationTokensDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().Append(kDatabaseFileName);
  }

  void TearDown() override {
    pvt_database_.reset();
    WaitForAllTasksPosted();
    EXPECT_TRUE(temp_dir_.Delete());
  }

  void VerifyTableRowCount(sql::Database& db, const char* table, size_t count) {
    size_t got_count = 0;
    ASSERT_TRUE(sql::test::CountTableRows(&db, table, &got_count));
    EXPECT_EQ(count, got_count);
  }

  size_t CountRedeemedTokens(sql::Database& db) {
    static const char kCountSQL[] =
        "SELECT COUNT(*) FROM tokens WHERE redeemed = 1";
    sql::Statement s(db.GetUniqueStatement(kCountSQL));
    EXPECT_TRUE(s.Step());
    return s.ColumnInt(0);
  }

  std::vector<PrivateVerificationTokensToken> GetAllTokens(sql::Database& db) {
    sql::Statement statement(db.GetUniqueStatement(
        "SELECT issuer, token, key_id, "
        "expiration, version, creation_time FROM tokens"));
    std::vector<PrivateVerificationTokensToken> tokens;
    while (statement.Step()) {
      std::string issuer_str = statement.ColumnString(0);
      SerializedToken token = statement.ColumnBlobAsVector(1);
      uint32_t key_id = statement.ColumnInt64(2);
      int64_t expiration = statement.ColumnInt64(3);
      uint32_t version = statement.ColumnInt64(4);
      int64_t creation_time = statement.ColumnInt64(5);
      tokens.emplace_back(
          url::Origin::Create(GURL(issuer_str)), std::move(token), key_id,
          base::Time::UnixEpoch() + base::Seconds(expiration), version,
          base::Time::UnixEpoch() + base::Seconds(creation_time));
    }
    return tokens;
  }

  void CreateDatabase(const base::FilePath& path) {
    pvt_database_.reset();
    pvt_database_ = PrivateVerificationTokensDatabase::Create(path);
    ASSERT_NE(pvt_database_, nullptr);
  }

  // Flushes the main thread task runner by posting a task and waiting for its
  // execution, guaranteeing that all previously posted tasks have run.
  void WaitForAllTasksPosted() {
    base::test::TestFuture<void> cleanup_future;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, cleanup_future.GetCallback());
    EXPECT_TRUE(cleanup_future.Wait());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  std::unique_ptr<PrivateVerificationTokensDatabase> pvt_database_;

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.com"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.com"));
  const url::Origin kOriginC = url::Origin::Create(GURL("https://c.com"));
  const url::Origin kOriginD = url::Origin::Create(GURL("https://d.com"));
  const url::Origin kOriginE = url::Origin::Create(GURL("https://e.com"));
  const url::Origin kOriginBTri = url::Origin::Create(GURL("https://b.tri"));
  const url::Origin kOriginCEee = url::Origin::Create(GURL("https://c.eee"));
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
       CreateSequenceBound_ValidPath_Success) {
  auto maybe_database = PrivateVerificationTokensDatabase::CreateSequenceBound(
      task_environment_.GetMainThreadTaskRunner(), db_path_);
  EXPECT_FALSE(maybe_database.is_null());
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       CreateSequenceBound_EmptyPath_Failure) {
  const base::FilePath database_path;
  ASSERT_TRUE(database_path.empty());
  auto maybe_database = PrivateVerificationTokensDatabase::CreateSequenceBound(
      task_environment_.GetMainThreadTaskRunner(), database_path);
  EXPECT_TRUE(maybe_database.is_null());
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       CreateSequenceBound_Unused_NoFileCreated) {
  auto maybe_database = PrivateVerificationTokensDatabase::CreateSequenceBound(
      task_environment_.GetMainThreadTaskRunner(), db_path_);
  ASSERT_FALSE(maybe_database.is_null());

  maybe_database.Reset();
  WaitForAllTasksPosted();
  EXPECT_FALSE(base::PathExists(db_path_));
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       CreateSequenceBound_Used_FileCreated) {
  auto maybe_database = PrivateVerificationTokensDatabase::CreateSequenceBound(
      task_environment_.GetMainThreadTaskRunner(), db_path_);
  ASSERT_FALSE(maybe_database.is_null());

  EXPECT_FALSE(base::PathExists(db_path_));

  base::test::TestFuture<bool> future;
  maybe_database.AsyncCall(&PrivateVerificationTokensDatabase::StoreTokens)
      .WithArgs(std::vector<PrivateVerificationTokensToken>{})
      .Then(future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_TRUE(base::PathExists(db_path_));
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       PathToDatabase_ValidPath_ReturnsPath) {
  CreateDatabase(db_path_);
  EXPECT_EQ(pvt_database_->PathToDatabase(), db_path_);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       StoreTokens_EmptyList_FileCreated) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  EXPECT_TRUE(pvt_database_->StoreTokens({}));
  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));

  // meta and tokens tables
  EXPECT_EQ(2u, sql::test::CountSQLTables(&database));
  EXPECT_EQ(4, VersionFromMetaTable(database));

  EXPECT_EQ(8u, sql::test::CountTableColumns(&database, kTokenTableName));
  VerifyTableRowCount(database, kTokenTableName, 0u);
}

TEST_F(PrivateVerificationTokensDatabaseTest, StoreTokens_SingleToken_Success) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  uint32_t key_id = 1;
  int64_t expiration = 7;
  uint32_t version = 1;
  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(
          kOriginA, {1, 2, 3}, key_id,
          base::Time::UnixEpoch() + base::Seconds(expiration), version),
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));
  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));

  // meta and tokens tables
  EXPECT_EQ(2u, sql::test::CountSQLTables(&database));
  EXPECT_EQ(4, VersionFromMetaTable(database));
  EXPECT_EQ(8u, sql::test::CountTableColumns(&database, kTokenTableName));
  VerifyTableRowCount(database, kTokenTableName, 1);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       StoreTokens_MultipleTokens_Success) {
  EXPECT_FALSE(base::PathExists(db_path_));

  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  std::map<url::Origin, std::vector<SerializedToken>> all_tokens = {
      {kOriginA, {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {kOriginB, {{4, 5, 6}}},
      {kOriginC, {{7, 8, 9}, {47, 48, 49}}},
      {kOriginD, {{10, 11, 12}, {20, 12, 13}, {30, 15, 16}, {40, 41, 42}}},
  };
  auto tokens_to_store =
      CreateTokens(all_tokens, /* key_id = */ 1,
                   /* expiration = */
                   base::Time::UnixEpoch() + base::Seconds(7),
                   /* version = */ 1);
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens_to_store));
  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(8u, sql::test::CountTableColumns(&database, kTokenTableName));
  std::vector<PrivateVerificationTokensToken> got_tokens =
      GetAllTokens(database);
  EXPECT_THAT(got_tokens, testing::UnorderedElementsAreArray(tokens_to_store));
}

TEST_F(PrivateVerificationTokensDatabaseTest, GetToken_ExistingToken_Success) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  uint32_t key_id = 1;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;
  std::map<url::Origin, std::vector<SerializedToken>> all_tokens = {
      {kOriginA, {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {kOriginB, {{4, 5, 6}}},
      {kOriginC, {{7, 8, 9}, {47, 48, 49}}},
      {kOriginD, {{10, 11, 12}, {20, 12, 13}, {30, 15, 16}, {40, 41, 42}}},
  };
  auto tokens_to_store = CreateTokens(all_tokens, key_id, expiration, version);
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens_to_store));
  auto result = pvt_database_->GetToken(kOriginA);
  ASSERT_TRUE(result.has_value());
  SerializedToken serialized_token = result->token.token();

  EXPECT_THAT(all_tokens.at(kOriginA), testing::Contains(serialized_token));
}

TEST_F(PrivateVerificationTokensDatabaseTest, GetToken_NoTokens_Failure) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(kOriginA, {1, 2, 3},
                                     /* key_id = */ 1, expiration,
                                     /* version = */ 1)};

  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));
  auto result = pvt_database_->GetToken(kOriginE);
  EXPECT_FALSE(result.has_value());
  pvt_database_.reset();

  // verify that token for a.com is still there.
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  std::vector<PrivateVerificationTokensToken> got_tokens =
      GetAllTokens(database);
  EXPECT_EQ(got_tokens, tokens);
  database.Close();
}

TEST_F(PrivateVerificationTokensDatabaseTest, SetRedeemed_ValidId_Success) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;
  std::map<url::Origin, std::vector<SerializedToken>> all_tokens = {
      {kOriginA, {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {kOriginB, {{4, 5, 6}}},
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(
      CreateTokens(all_tokens, key_id, expiration, version)));

  auto a_token = pvt_database_->GetToken(kOriginA);
  ASSERT_TRUE(a_token.has_value());
  EXPECT_TRUE(pvt_database_->SetRedeemed(a_token->id));
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(8u, sql::test::CountTableColumns(&database, kTokenTableName));
  VerifyTableRowCount(database, kTokenTableName, 4u);
  EXPECT_EQ(1u, CountRedeemedTokens(database));
  database.Close();
}

TEST_F(PrivateVerificationTokensDatabaseTest, SetRedeemed_NonExistentId_NoOp) {
  CreateDatabase(db_path_);

  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(kOriginA, {1, 2, 3},
                                     /* key_id = */ 678, expiration,
                                     /* version = */ 1)};
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));

  auto token1 = pvt_database_->GetToken(kOriginA);
  ASSERT_TRUE(token1.has_value());
  ASSERT_LT(token1->id, std::numeric_limits<int64_t>::max());
  // token with id (token1->id + 1) does not exist.
  EXPECT_TRUE(pvt_database_->SetRedeemed(token1->id + 1));
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(0u, CountRedeemedTokens(database));
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       DeleteRedeemedTokens_MultipleRedeemed_Success) {
  CreateDatabase(db_path_);

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;
  std::map<url::Origin, std::vector<SerializedToken>> all_tokens = {
      {kOriginA, {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {kOriginB, {{4, 5, 6}}},
      {kOriginC, {{7, 8, 9}, {47, 48, 49}}},
      {kOriginD, {{10, 11, 12}, {20, 12, 13}, {30, 15, 16}, {40, 41, 42}}},
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(
      CreateTokens(all_tokens, key_id, expiration, version)));

  auto token1 = pvt_database_->GetToken(kOriginA);
  ASSERT_TRUE(token1.has_value());
  EXPECT_TRUE(pvt_database_->SetRedeemed(token1->id));

  auto token2 = pvt_database_->GetToken(kOriginA);
  ASSERT_TRUE(token2.has_value());
  EXPECT_TRUE(pvt_database_->SetRedeemed(token2->id));

  auto token3 = pvt_database_->GetToken(kOriginB);
  ASSERT_TRUE(token3.has_value());
  EXPECT_TRUE(pvt_database_->SetRedeemed(token3->id));

  EXPECT_TRUE(pvt_database_->DeleteRedeemedTokens());
  // no tokens for b.com left
  EXPECT_FALSE(pvt_database_->GetToken(kOriginB).has_value());
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  // started with 10 tokens, 3 tokens are redeemed and deleted.
  VerifyTableRowCount(database, kTokenTableName, 7);
  EXPECT_EQ(0u, CountRedeemedTokens(database));
  database.Close();
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

  // Trigger the lazy-initialization by attempting to store a token.
  uint32_t key_id = 1;
  int64_t expiration = 7;
  uint32_t version = 1;
  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(
          kOriginA, {1, 2, 3}, key_id,
          base::Time::UnixEpoch() + base::Seconds(expiration), version),
  };

  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));
  pvt_database_.reset();

  // Expect that the corrupted database was razed and re-initialized and the new
  // database has one token.
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(8u, sql::test::CountTableColumns(&database, kTokenTableName));
  VerifyTableRowCount(database, kTokenTableName, 1u);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       InitializeDB_FailedToCreateDirectory) {
  base::FilePath file_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("file_not_dir"));
  ASSERT_TRUE(base::WriteFile(file_path, "content"));

  base::FilePath invalid_db_path = file_path.Append(kDatabaseFileName);
  CreateDatabase(invalid_db_path);

  // Call StoreTokens with empty vector to trigger dbfile write.
  EXPECT_FALSE(pvt_database_->StoreTokens({}));

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

  // Call StoreTokens with empty vector to trigger dbfile write.
  EXPECT_FALSE(pvt_database_->StoreTokens({}));

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
  EXPECT_DCHECK_DEATH_WITH(pvt_database_->StoreTokens({}),
                           "Unexpected Sqlite error");
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       LoadFromFile_VersionTooNew_RazedAndReinitialized) {
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(db_path_));
    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 5, 5));
  }

  CreateDatabase(db_path_);

  uint32_t key_id = 1;
  int64_t expiration = 7;
  uint32_t version = 1;
  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(
          kOriginA, {1, 2, 3}, key_id,
          base::Time::UnixEpoch() + base::Seconds(expiration), version),
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(8u, sql::test::CountTableColumns(&database, kTokenTableName));
  VerifyTableRowCount(database, kTokenTableName, 1u);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       GetTokensFromEach_MultipleSites_Success) {
  CreateDatabase(db_path_);

  const uint32_t key_id = 3;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(5);
  const uint32_t version = 1;

  std::map<url::Origin, std::vector<SerializedToken>> all_tokens = {
      {kOriginA, {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {kOriginB, {{4, 5, 6}}},
      {kOriginC, {{7, 8, 9}, {47, 48, 49}}},
      {kOriginD, {{10, 11, 12}, {20, 12, 13}, {30, 15, 16}, {40, 41, 42}}},
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(
      CreateTokens(all_tokens, key_id, expiration, version)));
  std::map<url::Origin, TokenWithId> tokens =
      pvt_database_->GetTokensFromEach();
  EXPECT_EQ(tokens.size(), 4u);
  EXPECT_THAT(all_tokens.at(kOriginA),
              testing::Contains(tokens.at(kOriginA).token.token()));
  EXPECT_THAT(all_tokens.at(kOriginB),
              testing::Contains(tokens.at(kOriginB).token.token()));
  EXPECT_THAT(all_tokens.at(kOriginC),
              testing::Contains(tokens.at(kOriginC).token.token()));
  EXPECT_THAT(all_tokens.at(kOriginD),
              testing::Contains(tokens.at(kOriginD).token.token()));
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       GetTokensFromEach_NoTokens_Success) {
  CreateDatabase(db_path_);
  // This call creates the DB file.
  std::map<url::Origin, TokenWithId> tokens =
      pvt_database_->GetTokensFromEach();
  pvt_database_.reset();
  ASSERT_TRUE(base::PathExists(db_path_));

  // Re-create db with the created file.
  CreateDatabase(db_path_);
  tokens = pvt_database_->GetTokensFromEach();
  EXPECT_TRUE(tokens.empty());
}

TEST_F(PrivateVerificationTokensDatabaseTest, DeleteTokens_Neither) {
  CreateDatabase(db_path_);

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;
  std::map<url::Origin, std::vector<SerializedToken>> all_tokens = {
      {kOriginA, {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {kOriginB, {{4, 5, 6}}},
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(
      CreateTokens(all_tokens, key_id, expiration, version)));

  EXPECT_TRUE(pvt_database_->DeleteTokens(base::Time(), base::Time::Max(),
                                          std::nullopt));
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  VerifyTableRowCount(database, kTokenTableName, 0);
  database.Close();
}

TEST_F(PrivateVerificationTokensDatabaseTest, DeleteTokens_EmptyOriginVector) {
  CreateDatabase(db_path_);

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;
  std::map<url::Origin, std::vector<SerializedToken>> all_tokens = {
      {kOriginA, {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {kOriginB, {{4, 5, 6}}},
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(
      CreateTokens(all_tokens, key_id, expiration, version)));

  // Passing an empty vector should delete nothing.
  EXPECT_TRUE(pvt_database_->DeleteTokens(base::Time(), base::Time::Max(),
                                          std::vector<url::Origin>{}));
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  VerifyTableRowCount(database, kTokenTableName, 4);
  database.Close();
}

TEST_F(PrivateVerificationTokensDatabaseTest, DeleteTokens_TimeOnly) {
  CreateDatabase(db_path_);

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;

  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);
  base::Time t3 = base::Time::UnixEpoch() + base::Seconds(30);

  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(kOriginA, {1, 2, 3}, key_id, expiration,
                                     version, t1),
      PrivateVerificationTokensToken(kOriginB, {4, 5, 6}, key_id, expiration,
                                     version, t2),
      PrivateVerificationTokensToken(kOriginC, {7, 8, 9}, key_id, expiration,
                                     version, t3),
      PrivateVerificationTokensToken(kOriginA, {10, 11, 12}, key_id, expiration,
                                     version, t2),
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));

  // Delete tokens created on/after t2.
  // This should delete tokens[1] ("b.com" @ t2), tokens[2] ("c.com" @ t3) and
  // tokens[3] ("a.com" @ t2).
  EXPECT_TRUE(pvt_database_->DeleteTokens(t2, base::Time::Max(), std::nullopt));

  pvt_database_.reset();
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(db_path_));
    VerifyTableRowCount(database, kTokenTableName, 1);
    std::vector<PrivateVerificationTokensToken> got_tokens =
        GetAllTokens(database);
    EXPECT_THAT(got_tokens, testing::UnorderedElementsAre(tokens[0]));
  }
}

TEST_F(PrivateVerificationTokensDatabaseTest, DeleteTokens_OriginOnly) {
  CreateDatabase(db_path_);

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;

  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);
  base::Time t3 = base::Time::UnixEpoch() + base::Seconds(30);

  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(kOriginA, {1, 2, 3}, key_id, expiration,
                                     version, t1),
      PrivateVerificationTokensToken(kOriginB, {4, 5, 6}, key_id, expiration,
                                     version, t2),
      PrivateVerificationTokensToken(kOriginC, {7, 8, 9}, key_id, expiration,
                                     version, t3),
      PrivateVerificationTokensToken(kOriginA, {10, 11, 12}, key_id, expiration,
                                     version, t2),
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));

  EXPECT_TRUE(pvt_database_->DeleteTokens(base::Time(), base::Time::Max(),
                                          std::vector<url::Origin>{kOriginA}));

  pvt_database_.reset();
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(db_path_));
    VerifyTableRowCount(database, kTokenTableName, 2);
    std::vector<PrivateVerificationTokensToken> got_tokens =
        GetAllTokens(database);
    EXPECT_THAT(got_tokens,
                testing::UnorderedElementsAre(tokens[1], tokens[2]));
  }
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       DeleteTokens_OriginOnlyChunkedDelete) {
  CreateDatabase(db_path_);

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;

  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);

  PrivateVerificationTokensToken unmatched_token(kOriginA, {1, 2, 3}, key_id,
                                                 expiration, version, t1);
  std::vector<url::Origin> many_origins;
  // A number of origins well above the chromium sqlite configured max
  // placeholder count.
  size_t many_origins_size = 32768 + 100;
  many_origins.reserve(many_origins_size);
  for (size_t i = 0; i < many_origins_size; i++) {
    many_origins.push_back(url::Origin::Create(
        GURL(base::StringPrintf("https://example-%zu.com", i))));
  }
  std::vector<PrivateVerificationTokensToken> tokens = {unmatched_token};
  tokens.reserve(many_origins_size + 1);
  for (auto origin : many_origins) {
    tokens.push_back(PrivateVerificationTokensToken(origin, {1, 2, 3}, key_id,
                                                    expiration, version, t1));
  }

  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));

  EXPECT_TRUE(pvt_database_->DeleteTokens(base::Time(), base::Time::Max(),
                                          many_origins));

  pvt_database_.reset();
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(db_path_));
    VerifyTableRowCount(database, kTokenTableName, 1);
    std::vector<PrivateVerificationTokensToken> got_tokens =
        GetAllTokens(database);
    EXPECT_THAT(got_tokens, testing::UnorderedElementsAre(unmatched_token));
  }
}

TEST_F(PrivateVerificationTokensDatabaseTest, DeleteTokens_TimeAndOrigin) {
  CreateDatabase(db_path_);

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;

  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);
  base::Time t3 = base::Time::UnixEpoch() + base::Seconds(30);

  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(kOriginA, {1, 2, 3}, key_id, expiration,
                                     version, t1),
      PrivateVerificationTokensToken(kOriginB, {4, 5, 6}, key_id, expiration,
                                     version, t2),
      PrivateVerificationTokensToken(kOriginC, {7, 8, 9}, key_id, expiration,
                                     version, t3),
      PrivateVerificationTokensToken(kOriginA, {10, 11, 12}, key_id, expiration,
                                     version, t2),
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));

  EXPECT_TRUE(pvt_database_->DeleteTokens(t2, base::Time::Max(),
                                          std::vector<url::Origin>{kOriginA}));

  pvt_database_.reset();
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(db_path_));
    VerifyTableRowCount(database, kTokenTableName, 3);
    std::vector<PrivateVerificationTokensToken> got_tokens =
        GetAllTokens(database);
    EXPECT_THAT(got_tokens,
                testing::UnorderedElementsAre(tokens[0], tokens[1], tokens[2]));
  }
}

TEST_F(PrivateVerificationTokensDatabaseTest, DeleteTokens_MultipleOrigins) {
  CreateDatabase(db_path_);

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;

  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);
  base::Time t3 = base::Time::UnixEpoch() + base::Seconds(30);

  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(kOriginA, {1, 2, 3}, key_id, expiration,
                                     version, t1),
      PrivateVerificationTokensToken(kOriginB, {4, 5, 6}, key_id, expiration,
                                     version, t2),
      PrivateVerificationTokensToken(kOriginC, {7, 8, 9}, key_id, expiration,
                                     version, t3),
      PrivateVerificationTokensToken(kOriginA, {10, 11, 12}, key_id, expiration,
                                     version, t2),
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));

  EXPECT_TRUE(pvt_database_->DeleteTokens(
      base::Time(), base::Time::Max(),
      std::vector<url::Origin>{kOriginA, kOriginC}));

  pvt_database_.reset();
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(db_path_));
    VerifyTableRowCount(database, kTokenTableName, 1);
    std::vector<PrivateVerificationTokensToken> got_tokens =
        GetAllTokens(database);
    EXPECT_THAT(got_tokens, testing::UnorderedElementsAre(tokens[1]));
  }
}

TEST_F(PrivateVerificationTokensDatabaseTest, DeleteTokens_TimeRange) {
  CreateDatabase(db_path_);

  uint32_t key_id = 678;
  const base::Time expiration = base::Time::UnixEpoch() + base::Seconds(7);
  uint32_t version = 1;

  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);
  base::Time t3 = base::Time::UnixEpoch() + base::Seconds(30);

  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(kOriginA, {1, 2, 3}, key_id, expiration,
                                     version, t1),
      PrivateVerificationTokensToken(kOriginB, {4, 5, 6}, key_id, expiration,
                                     version, t2),
      PrivateVerificationTokensToken(kOriginC, {7, 8, 9}, key_id, expiration,
                                     version, t3),
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));

  // Delete tokens created in range [t2, t3).
  // This should delete tokens[1] ("b.com" @ t2), but keep tokens[0] ("a.com" @
  // t1) and tokens[2] ("c.com" @ t3).
  EXPECT_TRUE(pvt_database_->DeleteTokens(t2, t3, std::nullopt));

  pvt_database_.reset();
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(db_path_));
    VerifyTableRowCount(database, kTokenTableName, 2);
    std::vector<PrivateVerificationTokensToken> got_tokens =
        GetAllTokens(database);
    EXPECT_THAT(got_tokens,
                testing::UnorderedElementsAre(tokens[0], tokens[2]));
  }
}

}  // namespace

}  // namespace private_verification_tokens
