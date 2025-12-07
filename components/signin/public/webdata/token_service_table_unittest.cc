// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/webdata/token_service_table.h"

#include <memory>
#include <string>

#include "base/containers/to_vector.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Time;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;
using TokenWithBindingKey = TokenServiceTable::TokenWithBindingKey;

class TokenServiceTableTest : public testing::Test {
 public:
  TokenServiceTableTest()
      : encryptor_(os_crypt_async::GetTestEncryptorForTesting()) {}

  TokenServiceTableTest(const TokenServiceTableTest&) = delete;
  TokenServiceTableTest& operator=(const TokenServiceTableTest&) = delete;

  ~TokenServiceTableTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<TokenServiceTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_, &encryptor_));
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  const os_crypt_async::Encryptor encryptor_;
  std::unique_ptr<TokenServiceTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_F(TokenServiceTableTest, TokenServiceGetAllRemoveAll) {
  std::map<std::string, TokenWithBindingKey> out_map;
  std::string service;
  std::string service2;
  service = "testservice";
  service2 = "othertestservice";
  bool should_reencrypt = false;

  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_TRUE(out_map.empty());

  // Check that get all tokens works
  EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni", {}));
  EXPECT_TRUE(table_->SetTokenForService(service2, "steak", {}));
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_EQ(TokenWithBindingKey("pepperoni"), out_map.find(service)->second);
  EXPECT_EQ(TokenWithBindingKey("steak"), out_map.find(service2)->second);
  out_map.clear();

  // Purge
  EXPECT_TRUE(table_->RemoveAllTokens());
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_TRUE(out_map.empty());

  // Check that you can still add it back in
  EXPECT_TRUE(table_->SetTokenForService(service, "cheese", {}));
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_EQ(TokenWithBindingKey("cheese"), out_map.find(service)->second);
}

TEST_F(TokenServiceTableTest, TokenServiceGetSet) {
  std::map<std::string, TokenWithBindingKey> out_map;
  std::string service;
  service = "testservice";
  bool should_reencrypt = false;

  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_TRUE(out_map.empty());

  EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni", {}));
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_EQ(TokenWithBindingKey("pepperoni"), out_map.find(service)->second);
  out_map.clear();

  // try blanking it - won't remove it from the db though!
  EXPECT_TRUE(table_->SetTokenForService(service, std::string(), {}));
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_EQ(TokenWithBindingKey(""), out_map.find(service)->second);
  out_map.clear();

  // try mutating it
  EXPECT_TRUE(table_->SetTokenForService(service, "ham", {}));
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_EQ(TokenWithBindingKey("ham"), out_map.find(service)->second);
}

TEST_F(TokenServiceTableTest, TokenServiceRemove) {
  std::map<std::string, TokenWithBindingKey> out_map;
  std::string service;
  std::string service2;
  service = "testservice";
  service2 = "othertestservice";
  bool should_reencrypt = false;

  EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni", {}));
  EXPECT_TRUE(table_->SetTokenForService(service2, "steak", {}));
  EXPECT_TRUE(table_->RemoveTokenForService(service));
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_EQ(0u, out_map.count(service));
  EXPECT_EQ(TokenWithBindingKey("steak"), out_map.find(service2)->second);
}

TEST_F(TokenServiceTableTest, TokenServiceRemoveOther) {
  EXPECT_TRUE(table_->SetTokenForService("a", "1", {}));
  EXPECT_TRUE(table_->SetTokenForService("b", "2", {}));
  EXPECT_TRUE(table_->SetTokenForService("c", "3", {}));

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(table_->RemoveOtherTokens({"a", "c", "zzz"}));

  std::map<std::string, TokenWithBindingKey> out_map;
  bool should_reencrypt = false;
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_THAT(out_map, UnorderedElementsAre(
                           std::make_pair("a", TokenWithBindingKey("1")),
                           std::make_pair("c", TokenWithBindingKey("3"))));
  histogram_tester.ExpectUniqueSample(
      "Signin.TokenTable.RemoveOtherTokensCount",
      /*sample=*/1, /*expected_bucket_count=*/1);
}

TEST_F(TokenServiceTableTest, TokenServiceRemoveOtherKeepNone) {
  EXPECT_TRUE(table_->SetTokenForService("a", "1", {}));
  EXPECT_TRUE(table_->SetTokenForService("b", "2", {}));
  EXPECT_TRUE(table_->SetTokenForService("c", "3", {}));

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(table_->RemoveOtherTokens({}));

  std::map<std::string, TokenWithBindingKey> out_map;
  bool should_reencrypt = false;
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_THAT(out_map, IsEmpty());
  histogram_tester.ExpectUniqueSample(
      "Signin.TokenTable.RemoveOtherTokensCount",
      /*sample=*/3, /*expected_bucket_count=*/1);
}

class TokenServiceTableRemoveOtherStressTest
    : public TokenServiceTableTest,
      public testing::WithParamInterface<size_t> {};

// Tests variable `services_to_keep` vector sizes in `RemoveOtherTokens()`.
TEST_P(TokenServiceTableRemoveOtherStressTest, TokenServiceRemoveOtherStress) {
  const size_t test_size = GetParam();

  std::vector<std::string> services_to_keep;
  for (size_t i = 0; i < test_size; ++i) {
    services_to_keep.push_back("keep_" + base::NumberToString(i));
    EXPECT_TRUE(
        table_->SetTokenForService(services_to_keep[i], "keep_token", {}));
    EXPECT_TRUE(table_->SetTokenForService("remove_" + base::NumberToString(i),
                                           "remove_token", {}));
  }

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(table_->RemoveOtherTokens(services_to_keep));

  std::map<std::string, TokenWithBindingKey> out_map;
  bool should_reencrypt = false;
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));

  std::vector<std::pair<std::string, TokenWithBindingKey>> expected_pairs =
      base::ToVector(services_to_keep, [](const std::string& service) {
        return std::make_pair(service, TokenWithBindingKey("keep_token"));
      });
  EXPECT_THAT(out_map, UnorderedElementsAreArray(expected_pairs));
  histogram_tester.ExpectUniqueSample(
      "Signin.TokenTable.RemoveOtherTokensCount",
      /*sample=*/test_size,
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(,
                         TokenServiceTableRemoveOtherStressTest,
                         testing::Values(0u, 1u, 10u, 42u, 100u));

TEST_F(TokenServiceTableTest, GetSetWithBidningKey) {
  bool should_reencrypt = false;
  std::map<std::string, TokenWithBindingKey> out_map;
  const std::string kService = "testservice";
  const std::vector<uint8_t> kBindingKey = {1, 4, 2};

  EXPECT_TRUE(table_->SetTokenForService(kService, "pepperoni", kBindingKey));
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_EQ(TokenWithBindingKey("pepperoni", kBindingKey),
            out_map.find(kService)->second);
  out_map.clear();

  // Override with a new token with a new binding key.
  const std::vector<uint8_t> kNewBindingKey = {4, 8, 15, 23};
  EXPECT_TRUE(table_->SetTokenForService(kService, "ham", kNewBindingKey));
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_EQ(TokenWithBindingKey("ham", kNewBindingKey),
            out_map.find(kService)->second);
  out_map.clear();

  // Override with a new token without a binding key.
  EXPECT_TRUE(table_->SetTokenForService(kService, "steak", {}));
  EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
            table_->GetAllTokens(&out_map, should_reencrypt));
  EXPECT_EQ(TokenWithBindingKey("steak"), out_map.find(kService)->second);
  out_map.clear();
}

TEST_F(TokenServiceTableTest, TokenMetrics) {
  const std::string service = "testservice";
  {
    base::HistogramTester histograms;
    EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni", {}));
    histograms.ExpectUniqueSample("Signin.TokenTable.SetTokenResult",
                                  /*kSuccess*/ 0, 1u);
  }
  {
    base::HistogramTester histograms;
    std::map<std::string, TokenWithBindingKey> out_map;
    bool should_reencrypt;
    EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
              table_->GetAllTokens(&out_map, should_reencrypt));
    histograms.ExpectUniqueSample("Signin.TokenTable.ReadTokenFromDBResult",
                                  /*READ_ONE_TOKEN_SUCCESS*/ 0, 1u);
  }
}

class TokenServiceTableEncryptionOptionsTest : public testing::Test {
 public:
  TokenServiceTableEncryptionOptionsTest()
      : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)) {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  os_crypt_async::Encryptor GetInstanceSync(
      os_crypt_async::Encryptor::Option option) {
    base::test::TestFuture<os_crypt_async::Encryptor> future;
    os_crypt_->GetInstance(future.GetCallback(), option);
    return future.Take();
  }

  base::ScopedTempDir temp_dir_;

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
};

TEST_F(TokenServiceTableEncryptionOptionsTest, TokenReencrypt) {
  const auto filename =
      temp_dir_.GetPath().AppendASCII("TestWebMutableDatabase");

  const std::string kService = "testservice";
  const std::string kService2 = "othertestservice";

  const std::vector<uint8_t> kBindingKey = {1, 4, 2};

  {
    TokenServiceTable table;
    WebDatabase db;
    db.AddTable(&table);
    const auto encryptor =
        GetInstanceSync(os_crypt_async::Encryptor::Option::kEncryptSyncCompat);
    ASSERT_EQ(sql::INIT_OK, db.Init(filename, &encryptor));

    std::map<std::string, TokenWithBindingKey> out_map;
    bool should_reencrypt = false;

    EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
              table.GetAllTokens(&out_map, should_reencrypt));
    EXPECT_FALSE(should_reencrypt);
    EXPECT_TRUE(out_map.empty());
    table.SetTokenForService(kService, "steak", kBindingKey);
  }

  {
    TokenServiceTable table;
    WebDatabase db;
    db.AddTable(&table);
    const auto encryptor =
        GetInstanceSync(os_crypt_async::Encryptor::Option::kNone);
    ASSERT_EQ(sql::INIT_OK, db.Init(filename, &encryptor));

    std::map<std::string, TokenWithBindingKey> out_map;
    bool should_reencrypt = false;

    EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
              table.GetAllTokens(&out_map, should_reencrypt));
    // Moving from OSCrypt Sync compatible to new encryption should result in a
    // re-encrypt being requested.
    EXPECT_TRUE(should_reencrypt);
    EXPECT_EQ(TokenWithBindingKey("steak", kBindingKey),
              out_map.find(kService)->second);
    // Write the new value back, re-encrypting it.
    table.SetTokenForService(kService, "steak", kBindingKey);
  }
  {
    TokenServiceTable table;
    WebDatabase db;
    db.AddTable(&table);
    const auto encryptor =
        GetInstanceSync(os_crypt_async::Encryptor::Option::kNone);
    ASSERT_EQ(sql::INIT_OK, db.Init(filename, &encryptor));

    std::map<std::string, TokenWithBindingKey> out_map;
    bool should_reencrypt = false;

    EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
              table.GetAllTokens(&out_map, should_reencrypt));
    EXPECT_FALSE(should_reencrypt);
    EXPECT_EQ(TokenWithBindingKey("steak", kBindingKey),
              out_map.find(kService)->second);
    // Add a second item.
    table.SetTokenForService(kService2, "pepperoni", kBindingKey);
  }
  {
    TokenServiceTable table;
    WebDatabase db;
    db.AddTable(&table);
    const auto encryptor =
        GetInstanceSync(os_crypt_async::Encryptor::Option::kNone);
    ASSERT_EQ(sql::INIT_OK, db.Init(filename, &encryptor));

    std::map<std::string, TokenWithBindingKey> out_map;
    bool should_reencrypt = false;

    EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
              table.GetAllTokens(&out_map, should_reencrypt));
    // Note: pepperoni was already encrypted with the new key, so did not need
    // to be re-encrypted.
    EXPECT_FALSE(should_reencrypt);
    EXPECT_EQ(TokenWithBindingKey("steak", kBindingKey),
              out_map.find(kService)->second);
    EXPECT_EQ(TokenWithBindingKey("pepperoni", kBindingKey),
              out_map.find(kService2)->second);
  }
  {
    TokenServiceTable table;
    WebDatabase db;
    db.AddTable(&table);
    const auto encryptor =
        GetInstanceSync(os_crypt_async::Encryptor::Option::kEncryptSyncCompat);
    ASSERT_EQ(sql::INIT_OK, db.Init(filename, &encryptor));

    std::map<std::string, TokenWithBindingKey> out_map;
    bool should_reencrypt = false;

    EXPECT_EQ(TokenServiceTable::Result::TOKEN_DB_RESULT_SUCCESS,
              table.GetAllTokens(&out_map, should_reencrypt));
    // Reverting back to OSCrypt Sync compatible should request all data be
    // re-encrypted again.
    EXPECT_TRUE(should_reencrypt);
    EXPECT_EQ(TokenWithBindingKey("steak", kBindingKey),
              out_map.find(kService)->second);
    EXPECT_EQ(TokenWithBindingKey("pepperoni", kBindingKey),
              out_map.find(kService2)->second);
  }
}
