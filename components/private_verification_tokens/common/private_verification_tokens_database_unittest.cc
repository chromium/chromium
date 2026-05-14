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

bool operator==(const PrivateVerificationTokensPublicKey& lhs,
                const PrivateVerificationTokensPublicKey& rhs) {
  return lhs.etld_plus_one() == rhs.etld_plus_one() &&
         lhs.public_key() == rhs.public_key() && lhs.key_id() == rhs.key_id() &&
         lhs.expiration() == rhs.expiration() && lhs.version() == rhs.version();
}

bool operator==(const PrivateVerificationTokensToken& lhs,
                const PrivateVerificationTokensToken& rhs) {
  return lhs.etld_plus_one() == rhs.etld_plus_one() &&
         lhs.token() == rhs.token() && lhs.key_id() == rhs.key_id() &&
         lhs.expiration() == rhs.expiration() && lhs.version() == rhs.version();
}

namespace {

static constexpr base::FilePath::CharType kDatabaseFileName[] =
    FILE_PATH_LITERAL("PrivateVerificationTokens");
static constexpr char kKeyTableName[] = "keys";
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
// etld_plus_one.
std::vector<PrivateVerificationTokensToken> CreateTokens(
    std::map<std::string, std::vector<SerializedToken>> token_map,
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
    sql::Statement statement(
        db.GetUniqueStatement("SELECT etld_plus_one, token, key_id, "
                              "expiration, version FROM tokens"));
    std::vector<PrivateVerificationTokensToken> tokens;
    while (statement.Step()) {
      std::string etld_plus_one = statement.ColumnString(0);
      SerializedToken token = statement.ColumnBlobAsVector(1);
      uint32_t key_id = statement.ColumnInt64(2);
      int64_t expiration = statement.ColumnInt64(3);
      uint32_t version = statement.ColumnInt64(4);
      tokens.emplace_back(
          std::move(etld_plus_one), std::move(token), key_id,
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(expiration)),
          version);
    }
    return tokens;
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
       StoreTokens_EmptyList_FileCreated) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  EXPECT_TRUE(pvt_database_->StoreTokens({}));
  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));

  // meta, tokens and keys tables
  EXPECT_EQ(3u, sql::test::CountSQLTables(&database));
  EXPECT_EQ(1, VersionFromMetaTable(database));

  EXPECT_EQ(7u, sql::test::CountTableColumns(&database, kTokenTableName));
  VerifyTableRowCount(database, kTokenTableName, 0u);
}

TEST_F(PrivateVerificationTokensDatabaseTest, StoreTokens_SingleToken_Success) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  const std::string etld_plus_one = "a.com";
  uint32_t key_id = 1;
  uint64_t expiration = 7;
  uint32_t version = 1;
  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(
          etld_plus_one, {1, 2, 3}, key_id,
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(expiration)),
          version),
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));
  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));

  // meta, tokens and keys tables
  EXPECT_EQ(3u, sql::test::CountSQLTables(&database));
  EXPECT_EQ(1, VersionFromMetaTable(database));
  EXPECT_EQ(7u, sql::test::CountTableColumns(&database, kTokenTableName));
  VerifyTableRowCount(database, kTokenTableName, 1);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       StoreTokens_MultipleTokens_Success) {
  EXPECT_FALSE(base::PathExists(db_path_));

  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  std::map<std::string, std::vector<SerializedToken>> all_tokens = {
      {"a.com", {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {"b.com", {{4, 5, 6}}},
      {"c.com", {{7, 8, 9}, {47, 48, 49}}},
      {"d.com", {{10, 11, 12}, {20, 12, 13}, {30, 15, 16}, {40, 41, 42}}},
  };
  auto tokens_to_store =
      CreateTokens(all_tokens, /* key_id = */ 1,
                   /* expiration = */
                   base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(7)),
                   /* version = */ 1);
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens_to_store));
  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(7u, sql::test::CountTableColumns(&database, kTokenTableName));
  std::vector<PrivateVerificationTokensToken> got_tokens =
      GetAllTokens(database);
  EXPECT_THAT(got_tokens, testing::UnorderedElementsAreArray(tokens_to_store));
}

TEST_F(PrivateVerificationTokensDatabaseTest, GetToken_ExistingToken_Success) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  uint32_t key_id = 1;
  const base::Time expiration =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(7));
  uint32_t version = 1;
  std::map<std::string, std::vector<SerializedToken>> all_tokens = {
      {"a.com", {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {"b.com", {{4, 5, 6}}},
      {"c.com", {{7, 8, 9}, {47, 48, 49}}},
      {"d.com", {{10, 11, 12}, {20, 12, 13}, {30, 15, 16}, {40, 41, 42}}},
  };
  auto tokens_to_store = CreateTokens(all_tokens, key_id, expiration, version);
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens_to_store));
  auto result = pvt_database_->GetToken("a.com");
  ASSERT_TRUE(result.has_value());
  SerializedToken serialized_token = result->token.token();

  EXPECT_THAT(all_tokens.at("a.com"), testing::Contains(serialized_token));
}

TEST_F(PrivateVerificationTokensDatabaseTest, GetToken_NoTokens_Failure) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  const base::Time expiration =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(7));
  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken("a.com", {1, 2, 3},
                                     /* key_id = */ 1, expiration,
                                     /* version = */ 1)};

  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));
  auto result = pvt_database_->GetToken("e.com");
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
  const base::Time expiration =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(7));
  uint32_t version = 1;
  std::map<std::string, std::vector<SerializedToken>> all_tokens = {
      {"a.com", {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {"b.com", {{4, 5, 6}}},
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(
      CreateTokens(all_tokens, key_id, expiration, version)));

  auto a_token = pvt_database_->GetToken("a.com");
  ASSERT_TRUE(a_token.has_value());
  EXPECT_TRUE(pvt_database_->SetRedeemed(a_token->id));
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(7u, sql::test::CountTableColumns(&database, kTokenTableName));
  VerifyTableRowCount(database, kTokenTableName, 4u);
  EXPECT_EQ(1u, CountRedeemedTokens(database));
  database.Close();
}

TEST_F(PrivateVerificationTokensDatabaseTest, SetRedeemed_NonExistentId_NoOp) {
  CreateDatabase(db_path_);

  const base::Time expiration =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(7));
  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken("a.com", {1, 2, 3},
                                     /* key_id = */ 678, expiration,
                                     /* version = */ 1)};
  EXPECT_TRUE(pvt_database_->StoreTokens(tokens));

  auto token1 = pvt_database_->GetToken("a.com");
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
  const base::Time expiration =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(7));
  uint32_t version = 1;
  std::map<std::string, std::vector<SerializedToken>> all_tokens = {
      {"a.com", {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {"b.com", {{4, 5, 6}}},
      {"c.com", {{7, 8, 9}, {47, 48, 49}}},
      {"d.com", {{10, 11, 12}, {20, 12, 13}, {30, 15, 16}, {40, 41, 42}}},
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(
      CreateTokens(all_tokens, key_id, expiration, version)));

  auto token1 = pvt_database_->GetToken("a.com");
  ASSERT_TRUE(token1.has_value());
  EXPECT_TRUE(pvt_database_->SetRedeemed(token1->id));

  auto token2 = pvt_database_->GetToken("a.com");
  ASSERT_TRUE(token2.has_value());
  EXPECT_TRUE(pvt_database_->SetRedeemed(token2->id));

  auto token3 = pvt_database_->GetToken("b.com");
  ASSERT_TRUE(token3.has_value());
  EXPECT_TRUE(pvt_database_->SetRedeemed(token3->id));

  EXPECT_TRUE(pvt_database_->DeleteRedeemedTokens());
  // no tokens for b.com left
  EXPECT_FALSE(pvt_database_->GetToken("b.com").has_value());
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  // started with 10 tokens, 3 tokens are redeemed and deleted.
  VerifyTableRowCount(database, kTokenTableName, 7);
  EXPECT_EQ(0u, CountRedeemedTokens(database));
  database.Close();
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       StoreKeys_ValidKeys_StoredAndRetrieved) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  std::vector<uint8_t> key_a = {1, 2, 3};
  std::vector<uint8_t> key_b = {4, 5, 6};
  const auto exp = base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(5));
  std::vector<PrivateVerificationTokensPublicKey> keys{
      PrivateVerificationTokensPublicKey("a.com", key_a, /*key_id=*/3,
                                         /*expiration=*/exp, /*version=*/1),
      PrivateVerificationTokensPublicKey("b.tri", key_b, /*key_id=*/4,
                                         /*expiration=*/exp, /*version=*/2),
  };
  EXPECT_TRUE(pvt_database_->StoreKeys(keys));

  std::vector<PrivateVerificationTokensPublicKey> got_keys =
      pvt_database_->GetKeys();
  EXPECT_THAT(got_keys, testing::UnorderedElementsAreArray(keys));
  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(5u, sql::test::CountTableColumns(&database, kKeyTableName));
  VerifyTableRowCount(database, kKeyTableName, 2u);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       StoreKeys_DuplicateKeys_Overwrites) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  std::vector<uint8_t> key_a = {1, 2, 3};
  const auto exp = base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(5));
  std::vector<PrivateVerificationTokensPublicKey> keys{
      PrivateVerificationTokensPublicKey("a.com", key_a, /*key_id=*/3,
                                         /*expiration=*/exp, /*version=*/1),
  };
  EXPECT_TRUE(pvt_database_->StoreKeys(keys));

  std::vector<uint8_t> key_a_new = {7, 8, 9};
  const auto exp_new =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(10));
  std::vector<PrivateVerificationTokensPublicKey> keys_new{
      PrivateVerificationTokensPublicKey("a.com", key_a_new, /*key_id=*/3,
                                         /*expiration=*/exp_new, /*version=*/2),
  };
  EXPECT_TRUE(pvt_database_->StoreKeys(keys_new));

  std::vector<PrivateVerificationTokensPublicKey> got_keys =
      pvt_database_->GetKeys();
  EXPECT_THAT(got_keys, testing::UnorderedElementsAreArray(keys_new));

  pvt_database_.reset();
  EXPECT_TRUE(base::PathExists(db_path_));
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(5u, sql::test::CountTableColumns(&database, kKeyTableName));
  VerifyTableRowCount(database, kKeyTableName, 1u);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       RemoveKeysFor_ExistingETLD_KeysRemoved) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  std::vector<uint8_t> key_a = {1, 2, 3};
  std::vector<uint8_t> key_b_1 = {4, 5, 6};
  std::vector<uint8_t> key_c = {7, 8, 9};
  std::vector<uint8_t> key_b_2 = {10, 11, 12};
  std::vector<uint8_t> key_b_3 = {13, 14, 15};
  // b.tri has 3 keys
  std::vector<PrivateVerificationTokensPublicKey> keys{
      PrivateVerificationTokensPublicKey(
          "a.com", key_a, /*key_id=*/3,
          /*expiration=*/
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(5)),
          /*version=*/1),
      PrivateVerificationTokensPublicKey(
          "b.tri", key_b_1, /*key_id=*/4,
          /*expiration=*/
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(6)),
          /*version=*/2),
      PrivateVerificationTokensPublicKey(
          "c.eee", key_c, /*key_id=*/4,
          /*expiration=*/
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(6)),
          /*version=*/2),
      PrivateVerificationTokensPublicKey(
          "b.tri", key_b_2, /*key_id=*/5,
          /*expiration=*/
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(6)),
          /*version=*/2),
      PrivateVerificationTokensPublicKey(
          "b.tri", key_b_3, /*key_id=*/6,
          /*expiration=*/
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(6)),
          /*version=*/2),
  };
  EXPECT_TRUE(pvt_database_->StoreKeys(keys));
  // this should remove all three keys for b.tri
  EXPECT_TRUE(pvt_database_->RemoveKeysFor("b.tri"));
  std::vector<PrivateVerificationTokensPublicKey> got_keys =
      pvt_database_->GetKeys();
  EXPECT_THAT(got_keys, testing::UnorderedElementsAre(keys[0], keys[2]));
  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(5u, sql::test::CountTableColumns(&database, kKeyTableName));
  VerifyTableRowCount(database, kKeyTableName, 2u);
}

TEST_F(PrivateVerificationTokensDatabaseTest, RemoveKey_ExistingId_KeyRemoved) {
  EXPECT_FALSE(base::PathExists(db_path_));
  CreateDatabase(db_path_);
  EXPECT_FALSE(base::PathExists(db_path_));

  std::vector<uint8_t> key_a = {1, 2, 3};
  std::vector<uint8_t> key_b = {4, 5, 6};
  std::vector<uint8_t> key_c = {7, 8, 9};
  std::vector<PrivateVerificationTokensPublicKey> keys{
      PrivateVerificationTokensPublicKey(
          "a.com", key_a, /*key_id=*/3,
          /*expiration=*/
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(5)),
          /*version=*/1),
      PrivateVerificationTokensPublicKey(
          "b.tri", key_b, /*key_id=*/4,
          /*expiration=*/
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(6)),
          /*version=*/2),
      PrivateVerificationTokensPublicKey(
          "c.eee", key_c, /*key_id=*/5,
          /*expiration=*/
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(6)),
          /*version=*/2),
  };
  EXPECT_TRUE(pvt_database_->StoreKeys(keys));
  EXPECT_TRUE(pvt_database_->RemoveKey("b.tri", 4));
  std::vector<PrivateVerificationTokensPublicKey> got_keys =
      pvt_database_->GetKeys();
  EXPECT_THAT(got_keys, testing::UnorderedElementsAre(keys[0], keys[2]));
  pvt_database_.reset();

  EXPECT_TRUE(base::PathExists(db_path_));
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(5u, sql::test::CountTableColumns(&database, kKeyTableName));
  VerifyTableRowCount(database, kKeyTableName, 2u);
}

TEST_F(PrivateVerificationTokensDatabaseTest, RemoveKey_NonExistentId_NoOp) {
  CreateDatabase(db_path_);

  std::vector<uint8_t> key_a = {1, 2, 3};
  std::vector<PrivateVerificationTokensPublicKey> keys{
      PrivateVerificationTokensPublicKey(
          "a.com", key_a, /*key_id=*/3,
          base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(5)), 1),
  };
  EXPECT_TRUE(pvt_database_->StoreKeys(keys));

  EXPECT_TRUE(pvt_database_->RemoveKey("a.com", 4));

  std::vector<PrivateVerificationTokensPublicKey> got_keys =
      pvt_database_->GetKeys();
  EXPECT_EQ(got_keys, keys);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       GetKeys_EmptyTable_ReturnsEmptyVector) {
  CreateDatabase(db_path_);
  std::vector<PrivateVerificationTokensPublicKey> keys =
      pvt_database_->GetKeys();
  EXPECT_TRUE(keys.empty());
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
  const auto exp = base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(5));
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
  EXPECT_EQ(5u, sql::test::CountTableColumns(&database, kKeyTableName));
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
  const auto exp = base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(5));
  std::vector<PrivateVerificationTokensPublicKey> keys{
      PrivateVerificationTokensPublicKey("a.com", key_a, 3, exp, 1),
  };
  EXPECT_TRUE(pvt_database_->StoreKeys(keys));
  pvt_database_.reset();

  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(db_path_));
  EXPECT_EQ(5u, sql::test::CountTableColumns(&database, kKeyTableName));
  VerifyTableRowCount(database, kKeyTableName, 1u);
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       GetTokensFromEach_MultipleSites_Success) {
  CreateDatabase(db_path_);

  const uint32_t key_id = 3;
  const base::Time expiration =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(5));
  const uint32_t version = 1;

  std::map<std::string, std::vector<SerializedToken>> all_tokens = {
      {"a.com", {{1, 2, 3}, {11, 12, 13}, {14, 15, 16}}},
      {"b.com", {{4, 5, 6}}},
      {"c.com", {{7, 8, 9}, {47, 48, 49}}},
      {"d.com", {{10, 11, 12}, {20, 12, 13}, {30, 15, 16}, {40, 41, 42}}},
  };
  EXPECT_TRUE(pvt_database_->StoreTokens(
      CreateTokens(all_tokens, key_id, expiration, version)));
  std::map<std::string, TokenWithId> tokens =
      pvt_database_->GetTokensFromEach();
  EXPECT_EQ(tokens.size(), 4u);
  EXPECT_THAT(all_tokens.at("a.com"),
              testing::Contains(tokens.at("a.com").token.token()));
  EXPECT_THAT(all_tokens.at("b.com"),
              testing::Contains(tokens.at("b.com").token.token()));
  EXPECT_THAT(all_tokens.at("c.com"),
              testing::Contains(tokens.at("c.com").token.token()));
  EXPECT_THAT(all_tokens.at("d.com"),
              testing::Contains(tokens.at("d.com").token.token()));
}

TEST_F(PrivateVerificationTokensDatabaseTest,
       GetTokensFromEach_NoTokens_Success) {
  CreateDatabase(db_path_);
  // This call creates the DB file.
  std::map<std::string, TokenWithId> tokens =
      pvt_database_->GetTokensFromEach();
  pvt_database_.reset();
  ASSERT_TRUE(base::PathExists(db_path_));

  // Re-create db with the created file.
  CreateDatabase(db_path_);
  tokens = pvt_database_->GetTokensFromEach();
  EXPECT_TRUE(tokens.empty());
}

}  // namespace

}  // namespace private_verification_tokens
