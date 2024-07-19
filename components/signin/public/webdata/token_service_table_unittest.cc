// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/webdata/token_service_table.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using TokenWithBindingKey = TokenServiceTable::TokenWithBindingKey;

class TokenServiceTableTest : public testing::Test {
 public:
  TokenServiceTableTest() = default;

  TokenServiceTableTest(const TokenServiceTableTest&) = delete;
  TokenServiceTableTest& operator=(const TokenServiceTableTest&) = delete;

  ~TokenServiceTableTest() override = default;

 protected:
  void SetUp() override {
    OSCryptMocker::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<TokenServiceTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TokenServiceTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_F(TokenServiceTableTest, TokenServiceGetAllRemoveAll) {
  std::map<std::string, TokenWithBindingKey> out_map;
  std::string service;
  std::string service2;
  service = "testservice";
  service2 = "othertestservice";

  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  // Check that get all tokens works
  EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni", {}));
  EXPECT_TRUE(table_->SetTokenForService(service2, "steak", {}));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(TokenWithBindingKey("pepperoni"), out_map.find(service)->second);
  EXPECT_EQ(TokenWithBindingKey("steak"), out_map.find(service2)->second);
  out_map.clear();

  // Purge
  EXPECT_TRUE(table_->RemoveAllTokens());
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  // Check that you can still add it back in
  EXPECT_TRUE(table_->SetTokenForService(service, "cheese", {}));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(TokenWithBindingKey("cheese"), out_map.find(service)->second);
}

TEST_F(TokenServiceTableTest, TokenServiceGetSet) {
  std::map<std::string, TokenWithBindingKey> out_map;
  std::string service;
  service = "testservice";

  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni", {}));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(TokenWithBindingKey("pepperoni"), out_map.find(service)->second);
  out_map.clear();

  // try blanking it - won't remove it from the db though!
  EXPECT_TRUE(table_->SetTokenForService(service, std::string(), {}));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(TokenWithBindingKey(""), out_map.find(service)->second);
  out_map.clear();

  // try mutating it
  EXPECT_TRUE(table_->SetTokenForService(service, "ham", {}));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(TokenWithBindingKey("ham"), out_map.find(service)->second);
}

TEST_F(TokenServiceTableTest, TokenServiceRemove) {
  std::map<std::string, TokenWithBindingKey> out_map;
  std::string service;
  std::string service2;
  service = "testservice";
  service2 = "othertestservice";

  EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni", {}));
  EXPECT_TRUE(table_->SetTokenForService(service2, "steak", {}));
  EXPECT_TRUE(table_->RemoveTokenForService(service));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(0u, out_map.count(service));
  EXPECT_EQ(TokenWithBindingKey("steak"), out_map.find(service2)->second);
}

TEST_F(TokenServiceTableTest, GetSetWithBidningKey) {
  std::map<std::string, TokenWithBindingKey> out_map;
  const std::string kService = "testservice";
  const std::vector<uint8_t> kBindingKey = {1, 4, 2};

  EXPECT_TRUE(table_->SetTokenForService(kService, "pepperoni", kBindingKey));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(TokenWithBindingKey("pepperoni", kBindingKey),
            out_map.find(kService)->second);
  out_map.clear();

  // Override with a new token with a new binding key.
  const std::vector<uint8_t> kNewBindingKey = {4, 8, 15, 23};
  EXPECT_TRUE(table_->SetTokenForService(kService, "ham", kNewBindingKey));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(TokenWithBindingKey("ham", kNewBindingKey),
            out_map.find(kService)->second);
  out_map.clear();

  // Override with a new token without a binding key.
  EXPECT_TRUE(table_->SetTokenForService(kService, "steak", {}));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(TokenWithBindingKey("steak"), out_map.find(kService)->second);
  out_map.clear();
}
