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
#include "components/private_verification_tokens/common/private_verification_tokens_token.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace private_verification_tokens {

namespace {

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
      const std::map<url::Origin, std::vector<PrivateVerificationTokensToken>>&
          tokens) {
    std::vector<PrivateVerificationTokensToken> tt;
    for (const auto& tv : tokens) {
      for (const auto& tvt : tv.second) {
        tt.push_back(tvt);
      }
    }
    std::unique_ptr<PrivateVerificationTokensDatabase> database =
        PrivateVerificationTokensDatabase::Create(path);
    ASSERT_THAT(database, testing::NotNull());
    database->StoreTokens(std::move(tt));
    database.reset();
  }

  // Creates a fixed tokens map.
  std::map<url::Origin, std::vector<PrivateVerificationTokensToken>>
  CreateTestData() {
    const auto expiration = base::Time::UnixEpoch() + base::Seconds(27);
    const url::Origin a_origin = url::Origin::Create(GURL("https://a.com"));
    const url::Origin b_origin = url::Origin::Create(GURL("https://b.tri"));
    const url::Origin c_origin = url::Origin::Create(GURL("https://c.eee"));
    return {
        {a_origin,
         // has 2 tokens
         std::vector<PrivateVerificationTokensToken>{
             PrivateVerificationTokensToken(a_origin, {11, 22, 33},
                                            /*key_id=*/3, expiration,
                                            /*version=*/3),
             PrivateVerificationTokensToken(a_origin, {11, 22, 44},
                                            /*key_id=*/3, expiration,
                                            /*version=*/3),
         }},
        {b_origin,
         // has 2 tokens
         {
             PrivateVerificationTokensToken(b_origin, {11, 22, 55},
                                            /*key_id=*/3, expiration,
                                            /*version=*/3),
             PrivateVerificationTokensToken(b_origin, {11, 22, 66},
                                            /*key_id=*/3, expiration,
                                            /*version=*/3),
         }},
        // has a single token
        {c_origin,
         {PrivateVerificationTokensToken(c_origin, {11, 22, 77}, /*key_id=*/3,
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

  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.com"));
  const url::Origin kOriginBTri = url::Origin::Create(GURL("https://b.tri"));
  const url::Origin kOriginCEee = url::Origin::Create(GURL("https://c.eee"));
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
  // wait for cache to be initialized
  EXPECT_TRUE(future.Wait());
  // starting db file is empty, waiting should not change cache
  EXPECT_EQ(store->tokens().size(), 0u);
  store.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(database_path));
}

TEST_F(PrivateVerificationTokensStoreTest,
       CreateSuccess_InitializeFromExistingDatabaseFile) {
  const base::FilePath database_path = DbPath(TempDir());
  std::map<url::Origin, std::vector<PrivateVerificationTokensToken>> tokens =
      CreateTestData();
  StoreInDatabase(database_path, tokens);
  ASSERT_FALSE(database_path.empty());

  base::test::TestFuture<void> future;
  std::unique_ptr<PrivateVerificationTokensStore> store =
      PrivateVerificationTokensStore::Create(database_path,
                                             future.GetCallback());
  ASSERT_THAT(store, testing::NotNull());
  // check cache before wait
  EXPECT_EQ(store->tokens().size(), 0u);
  // wait for cache to be initialized
  EXPECT_TRUE(future.Wait());
  // verify cache size after wait
  EXPECT_EQ(store->tokens().size(), 3u);

  const url::Origin a_origin = url::Origin::Create(GURL("https://a.com"));
  const url::Origin b_origin = url::Origin::Create(GURL("https://b.tri"));
  const url::Origin c_origin = url::Origin::Create(GURL("https://c.eee"));

  // verify tokens in cache
  EXPECT_THAT(tokens.at(a_origin),
              testing::Contains(store->tokens().at(a_origin).token));
  EXPECT_THAT(tokens.at(b_origin),
              testing::Contains(store->tokens().at(b_origin).token));
  EXPECT_EQ(tokens.at(c_origin)[0], store->tokens().at(c_origin).token);

  store.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();
  // re-read database and check
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(database_path));
  // Verify that all stored tokens remain in the database.
  VerifyTableRowCount(database, kTokenTableName, 5u);
}

TEST_F(PrivateVerificationTokensStoreTest, DeleteAllTokens_Success) {
  const base::FilePath database_path = DbPath(TempDir());
  std::map<url::Origin, std::vector<PrivateVerificationTokensToken>> tokens =
      CreateTestData();
  StoreInDatabase(database_path, tokens);

  CreateStore(database_path);
  ASSERT_EQ(store()->tokens().size(), 3u);

  store()->DeleteAllTokens();
  // Cache should be cleared immediately.
  EXPECT_EQ(store()->tokens().size(), 0u);

  // Wait for the async db call to finish.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Verify memory cache is still empty.
  EXPECT_EQ(store()->tokens().size(), 0u);

  store_.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Re-read database and check that it's empty in DB as well.
  sql::Database database(sql::test::kTestTag);
  EXPECT_TRUE(database.Open(database_path));
  VerifyTableRowCount(database, kTokenTableName, 0u);
}

TEST_F(PrivateVerificationTokensStoreTest, DeleteTokens_TimeOnly) {
  const base::FilePath database_path = DbPath(TempDir());

  const auto expiration = base::Time::UnixEpoch() + base::Seconds(27);
  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);

  std::map<url::Origin, std::vector<PrivateVerificationTokensToken>> tokens = {
      {kOriginA,
       {PrivateVerificationTokensToken(kOriginA, {11, 22, 33}, /*key_id=*/3,
                                       expiration, /*version=*/3,
                                       /*creation_time=*/t1)}},
      {kOriginBTri,
       {PrivateVerificationTokensToken(kOriginBTri, {11, 22, 55}, /*key_id=*/3,
                                       expiration, /*version=*/3,
                                       /*creation_time=*/t2)}},
  };

  StoreInDatabase(database_path, tokens);

  CreateStore(database_path);
  ASSERT_EQ(store()->tokens().size(), 2u);

  // Delete tokens created on/after t2.
  base::test::TestFuture<void> future;
  store()->DeleteTokens(t2, base::Time::Max(), std::nullopt,
                        future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // "b.tri" token was created at t2, so it should be gone.
  EXPECT_EQ(store()->tokens().size(), 1u);
  EXPECT_TRUE(store()->tokens().contains(kOriginA));
  EXPECT_FALSE(store()->tokens().contains(kOriginBTri));

  // Reset store to close DB connection before reading DB.
  store_.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Re-read DB.
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(database_path));
    VerifyTableRowCount(database, kTokenTableName, 1u);
  }
}

TEST_F(PrivateVerificationTokensStoreTest,
       DeleteTokens_EmptyOriginVectorTerminatesEarly) {
  const base::FilePath database_path = DbPath(TempDir());
  std::map<url::Origin, std::vector<PrivateVerificationTokensToken>> tokens =
      CreateTestData();
  StoreInDatabase(database_path, tokens);

  CreateStore(database_path);
  ASSERT_EQ(store()->tokens().size(), 3u);

  // Passing an empty vector of origins should terminate early and delete
  // nothing.
  base::test::TestFuture<void> future;
  store()->DeleteTokens(base::Time(), base::Time::Max(),
                        std::vector<url::Origin>{}, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // Memory cache and DB should remain unchanged.
  EXPECT_EQ(store()->tokens().size(), 3u);

  store_.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(database_path));
    VerifyTableRowCount(database, kTokenTableName, 5u);
  }
}

TEST_F(PrivateVerificationTokensStoreTest, DeleteTokens_OriginOnly) {
  const base::FilePath database_path = DbPath(TempDir());

  const auto expiration = base::Time::UnixEpoch() + base::Seconds(27);
  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);

  std::map<url::Origin, std::vector<PrivateVerificationTokensToken>> tokens = {
      {kOriginA,
       {PrivateVerificationTokensToken(kOriginA, {11, 22, 33}, /*key_id=*/3,
                                       expiration, /*version=*/3,
                                       /*creation_time=*/t1)}},
      {kOriginBTri,
       {PrivateVerificationTokensToken(kOriginBTri, {11, 22, 55}, /*key_id=*/3,
                                       expiration, /*version=*/3,
                                       /*creation_time=*/t2)}},
  };

  StoreInDatabase(database_path, tokens);

  CreateStore(database_path);
  ASSERT_EQ(store()->tokens().size(), 2u);

  // Delete by site.
  base::test::TestFuture<void> future;
  store()->DeleteTokens(base::Time(), base::Time::Max(),
                        std::vector<url::Origin>{kOriginBTri},
                        future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // "b.tri" token should be gone.
  EXPECT_EQ(store()->tokens().size(), 1u);
  EXPECT_TRUE(store()->tokens().contains(kOriginA));
  EXPECT_FALSE(store()->tokens().contains(kOriginBTri));

  // Reset store to close DB connection before reading DB.
  store_.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Re-read DB.
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(database_path));
    VerifyTableRowCount(database, kTokenTableName, 1u);
  }
}

TEST_F(PrivateVerificationTokensStoreTest, DeleteTokens_TimeAndOrigin) {
  const base::FilePath database_path = DbPath(TempDir());

  const auto expiration = base::Time::UnixEpoch() + base::Seconds(27);
  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);

  std::map<url::Origin, std::vector<PrivateVerificationTokensToken>> tokens = {
      {kOriginA,
       {PrivateVerificationTokensToken(kOriginA, {11, 22, 33}, /*key_id=*/3,
                                       expiration, /*version=*/3,
                                       /*creation_time=*/t1)}},
      {kOriginBTri,
       {PrivateVerificationTokensToken(kOriginBTri, {11, 22, 55}, /*key_id=*/3,
                                       expiration, /*version=*/3,
                                       /*creation_time=*/t2)}},
  };

  StoreInDatabase(database_path, tokens);

  CreateStore(database_path);
  ASSERT_EQ(store()->tokens().size(), 2u);

  // Delete by time and site.
  // Delete tokens created on/after t1 for "a.com".
  base::test::TestFuture<void> future;
  store()->DeleteTokens(t1, base::Time::Max(),
                        std::vector<url::Origin>{kOriginA},
                        future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // "a.com" token was created at t1, so it should be gone.
  EXPECT_EQ(store()->tokens().size(), 1u);
  EXPECT_FALSE(store()->tokens().contains(kOriginA));
  EXPECT_TRUE(store()->tokens().contains(kOriginBTri));

  // Reset store to close DB connection before reading DB.
  store_.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Re-read DB.
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(database_path));
    VerifyTableRowCount(database, kTokenTableName, 1u);
  }
}

TEST_F(PrivateVerificationTokensStoreTest, DeleteTokens_MultipleOrigins) {
  const base::FilePath database_path = DbPath(TempDir());

  const auto expiration = base::Time::UnixEpoch() + base::Seconds(27);
  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);
  base::Time t3 = base::Time::UnixEpoch() + base::Seconds(30);

  std::map<url::Origin, std::vector<PrivateVerificationTokensToken>> tokens = {
      {kOriginA,
       {PrivateVerificationTokensToken(kOriginA, {11, 22, 33}, /*key_id=*/3,
                                       expiration, /*version=*/3,
                                       /*creation_time=*/t1)}},
      {kOriginBTri,
       {PrivateVerificationTokensToken(kOriginBTri, {11, 22, 55}, /*key_id=*/3,
                                       expiration, /*version=*/3,
                                       /*creation_time=*/t2)}},
      {kOriginCEee,
       {PrivateVerificationTokensToken(kOriginCEee, {11, 22, 77}, /*key_id=*/3,
                                       expiration, /*version=*/3,
                                       /*creation_time=*/t3)}},
  };

  StoreInDatabase(database_path, tokens);

  CreateStore(database_path);
  ASSERT_EQ(store()->tokens().size(), 3u);

  // Delete two out of three sites.
  base::test::TestFuture<void> future;
  store()->DeleteTokens(base::Time(), base::Time::Max(),
                        std::vector<url::Origin>{kOriginA, kOriginCEee},
                        future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // "a.com" and "c.eee" should be gone.
  EXPECT_EQ(store()->tokens().size(), 1u);
  EXPECT_TRUE(store()->tokens().contains(kOriginBTri));
  EXPECT_FALSE(store()->tokens().contains(kOriginA));
  EXPECT_FALSE(store()->tokens().contains(kOriginCEee));

  // Reset store to close DB connection before reading DB.
  store_.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Re-read DB.
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(database_path));
    VerifyTableRowCount(database, kTokenTableName, 1u);
  }
}

TEST_F(PrivateVerificationTokensStoreTest, DeleteTokens_TimeRange) {
  const base::FilePath database_path = DbPath(TempDir());

  const auto expiration = base::Time::UnixEpoch() + base::Seconds(27);
  base::Time t1 = base::Time::UnixEpoch() + base::Seconds(10);
  base::Time t2 = base::Time::UnixEpoch() + base::Seconds(20);
  base::Time t3 = base::Time::UnixEpoch() + base::Seconds(30);

  std::map<url::Origin, std::vector<PrivateVerificationTokensToken>>
      test_tokens = {
          {kOriginA,
           {PrivateVerificationTokensToken(kOriginA, {11, 22, 33}, /*key_id=*/3,
                                           expiration, /*version=*/3,
                                           /*creation_time=*/t1)}},
          {kOriginBTri,
           {PrivateVerificationTokensToken(kOriginBTri, {11, 22, 55},
                                           /*key_id=*/3, expiration,
                                           /*version=*/3,
                                           /*creation_time=*/t2)}},
          {kOriginCEee,
           {PrivateVerificationTokensToken(kOriginCEee, {11, 22, 77},
                                           /*key_id=*/3, expiration,
                                           /*version=*/3,
                                           /*creation_time=*/t3)}},
      };

  StoreInDatabase(database_path, test_tokens);

  CreateStore(database_path);
  ASSERT_EQ(store()->tokens().size(), 3u);

  // Delete by range [t2, t3).
  // "b.tri" token was created at t2, so it should be gone.
  // "a.com" (@ t1) and "c.eee" (@ t3) should remain.
  base::test::TestFuture<void> future;
  store()->DeleteTokens(t2, t3, std::nullopt, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(store()->tokens().size(), 2u);
  EXPECT_TRUE(store()->tokens().contains(kOriginA));
  EXPECT_FALSE(store()->tokens().contains(kOriginBTri));
  EXPECT_TRUE(store()->tokens().contains(kOriginCEee));

  // Reset store to close DB connection before reading DB.
  store_.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Re-read DB.
  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(database_path));
    VerifyTableRowCount(database, kTokenTableName, 2u);
  }
}

TEST_F(PrivateVerificationTokensStoreTest, StoreTokens_Success) {
  const base::FilePath database_path = DbPath(TempDir());
  CreateStore(database_path);
  ASSERT_EQ(store()->tokens().size(), 0u);

  const auto expiration = base::Time::Now() + base::Hours(2);
  std::vector<PrivateVerificationTokensToken> tokens = {
      PrivateVerificationTokensToken(kOriginA, {1, 2, 3}, 1, expiration, 1),
      PrivateVerificationTokensToken(kOriginBTri, {4, 5, 6}, 2, expiration, 1),
  };

  base::test::TestFuture<void> future;
  store()->StoreTokens(std::move(tokens), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(store()->tokens().size(), 2u);
  EXPECT_TRUE(store()->tokens().contains(kOriginA));
  EXPECT_TRUE(store()->tokens().contains(kOriginBTri));

  store_.reset();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  {
    sql::Database database(sql::test::kTestTag);
    ASSERT_TRUE(database.Open(database_path));
    VerifyTableRowCount(database, kTokenTableName, 2u);
  }
}

}  // namespace

}  // namespace private_verification_tokens
