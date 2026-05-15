// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_store.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_verification_tokens {

namespace {

static constexpr char kKeyTableName[] = "keys";
static constexpr char kTokenTableName[] = "tokens";

class PrivateVerificationTokensStoreTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::ScopedTempDir& TempDir() const { return temp_dir_; }

  base::FilePath DbPath(const base::ScopedTempDir& temp_dir) const {
    return temp_dir.GetPath().Append(
        FILE_PATH_LITERAL("PrivateVerificationTokens"));
  }

  void VerifyTableRowCount(sql::Database& db, const char* table, size_t count) {
    size_t got_count = 0;
    ASSERT_TRUE(sql::test::CountTableRows(&db, table, &got_count));
    EXPECT_EQ(count, got_count);
  }

  void StoreInDatabase(
      const base::FilePath& path,
      const std::map<std::string, PrivateVerificationTokensPublicKey>& keys,
      const std::map<std::string, std::vector<PrivateVerificationTokensToken>>&
          tokens) {
    std::vector<PrivateVerificationTokensPublicKey> kk;
    for (const auto& k : keys) {
      kk.push_back(k.second);
    }
    std::vector<PrivateVerificationTokensToken> tt;
    for (const auto& tv : tokens) {
      for (const auto& tvt : tv.second) {
        tt.push_back(tvt);
      }
    }
    std::unique_ptr<PrivateVerificationTokensDatabase> database =
        PrivateVerificationTokensDatabase::Create(path);
    ASSERT_THAT(database, testing::NotNull());
    database->StoreKeys(std::move(kk));
    database->StoreTokens(std::move(tt));
    database.reset();
  }

  // Creates a fixed keys and tokens map.
  void CreateTestData(
      std::map<std::string, PrivateVerificationTokensPublicKey>& keys,
      std::map<std::string, std::vector<PrivateVerificationTokensToken>>&
          tokens) {
    const auto expiration =
        base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(27));
    keys = {
        {"a.com",
         PrivateVerificationTokensPublicKey("a.com", {1, 2, 3}, /*key_id=*/3,
                                            expiration, /*version=*/1)},
        {"b.tri",
         PrivateVerificationTokensPublicKey("b.tri", {4, 5, 6}, /*key_id=*/4,
                                            expiration, /*version=*/2)},
        {"c.eee",
         PrivateVerificationTokensPublicKey("c.eee", {7, 8, 9}, /*key_id=*/4,
                                            expiration, /*version=*/2)},
    };
    tokens = {
        {"a.com",
         // has 2 tokens
         std::vector<PrivateVerificationTokensToken>{
             PrivateVerificationTokensToken("a.com", {11, 22, 33}, /*key_id=*/3,
                                            expiration, /*version=*/3),
             PrivateVerificationTokensToken("a.com", {11, 22, 44}, /*key_id=*/3,
                                            expiration, /*version=*/3),
         }},
        {"b.tri",
         // has 2 tokens
         {
             PrivateVerificationTokensToken("b.tri", {11, 22, 55}, /*key_id=*/3,
                                            expiration, /*version=*/3),
             PrivateVerificationTokensToken("b.tri", {11, 22, 66}, /*key_id=*/3,
                                            expiration, /*version=*/3),
         }},
        // has a single token
        {"c.eee",
         {PrivateVerificationTokensToken("c.eee", {11, 22, 77}, /*key_id=*/3,
                                         expiration, /*version=*/3)}},
    };
  }

  // Creates a store and waits for cache to be initialized.
  void CreateStore(const base::FilePath& path) {
    store_.reset();
    base::test::TestFuture<void> future;
    store_ = PrivateVerificationTokensStore::Create(path, future.GetCallback());
    ASSERT_THAT(store_, testing::NotNull());
    // wait for cache to be initialized
    EXPECT_TRUE(future.Wait());
  }

  PrivateVerificationTokensStore* store() { return store_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<PrivateVerificationTokensStore> store_;
};

TEST_F(PrivateVerificationTokensStoreTest, Create_ValidPath_Success) {
  const base::FilePath database_path = DbPath(TempDir());
  CreateStore(database_path);
  EXPECT_THAT(store_, testing::NotNull());
}

TEST_F(PrivateVerificationTokensStoreTest, Create_EmptyPath_Failure) {
  const base::FilePath database_path;
  ASSERT_TRUE(database_path.empty());
  std::unique_ptr<PrivateVerificationTokensStore> store =
      PrivateVerificationTokensStore::Create(database_path, base::DoNothing());
  EXPECT_THAT(store, testing::IsNull());
}

TEST_F(PrivateVerificationTokensStoreTest, Create_Unused_NoFileCreated) {
  const base::FilePath database_path = DbPath(TempDir());
  base::test::TestFuture<void> future;
  std::unique_ptr<PrivateVerificationTokensStore> store =
      PrivateVerificationTokensStore::Create(database_path,
                                             future.GetCallback());
  ASSERT_THAT(store, testing::NotNull());
  EXPECT_EQ(store->tokens().size(), 0u);
  EXPECT_EQ(store->public_keys().size(), 0u);
  // wait for cache to be initialized
  EXPECT_TRUE(future.Wait());
  // starting db file is empty, waiting should not change cache
  EXPECT_EQ(store->tokens().size(), 0u);
  EXPECT_EQ(store->public_keys().size(), 0u);
  store.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(database_path));
}

TEST_F(PrivateVerificationTokensStoreTest,
       CreateSuccess_InitializeFromExistingDatabaseFile) {
  const base::FilePath database_path = DbPath(TempDir());
  std::map<std::string, PrivateVerificationTokensPublicKey> keys;
  std::map<std::string, std::vector<PrivateVerificationTokensToken>> tokens;
  // creates 3 distinct etld, 5 tokens
  CreateTestData(keys, tokens);
  StoreInDatabase(database_path, keys, tokens);
  ASSERT_FALSE(database_path.empty());

  base::test::TestFuture<void> future;
  std::unique_ptr<PrivateVerificationTokensStore> store =
      PrivateVerificationTokensStore::Create(database_path,
                                             future.GetCallback());
  ASSERT_THAT(store, testing::NotNull());
  // check cache before wait
  EXPECT_EQ(store->tokens().size(), 0u);
  EXPECT_EQ(store->public_keys().size(), 0u);
  // wait for cache to be initialized
  EXPECT_TRUE(future.Wait());
  // verify cache size after wait
  EXPECT_EQ(store->tokens().size(), 3u);
  EXPECT_EQ(store->public_keys().size(), 3u);

  // verify tokens in cache
  EXPECT_THAT(tokens.at("a.com"),
              testing::Contains(store->tokens().at("a.com").token));
  EXPECT_THAT(tokens.at("b.tri"),
              testing::Contains(store->tokens().at("b.tri").token));
  EXPECT_EQ(tokens.at("c.eee")[0], store->tokens().at("c.eee").token);

  // verify keys in cache
  EXPECT_EQ(keys.at("a.com"), store->public_keys().at("a.com"));
  EXPECT_EQ(keys.at("b.tri"), store->public_keys().at("b.tri"));
  EXPECT_EQ(keys.at("c.eee"), store->public_keys().at("c.eee"));

  store.reset();
  // re-read database and check
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(database_path));
  // Verify that all stored tokens remain in the database.
  VerifyTableRowCount(database, kTokenTableName, 5u);
  VerifyTableRowCount(database, kKeyTableName, 3u);
}

}  // namespace

}  // namespace private_verification_tokens
