// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/token_table.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_manager {
namespace {

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("TokenTableTest.db");

class TokenTableTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<sql::Database>(sql::DatabaseOptions());
    ASSERT_TRUE(InitDb(*db_));
  }

  void TearDown() override {
    db_->Close();
    EXPECT_TRUE(temp_dir_.Delete());
  }

  base::FilePath db_file_path() {
    return temp_dir_.GetPath().Append(kDatabaseName);
  }

  bool InitDb(sql::Database& db) {
    if (db.is_open()) {
      return true;
    }
    if (!db.Open(db_file_path())) {
      return false;
    }
    return true;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> db_;
};

TEST_F(TokenTableTest, Init) {
  TokenTable table(db_.get());
  EXPECT_TRUE(table.Init());
}

TEST_F(TokenTableTest, GetTokenId) {
  TokenTable table(db_.get());
  EXPECT_TRUE(table.Init());

  EXPECT_EQ(table.GetTokenId("hello"), -1);
  EXPECT_EQ(table.GetOrCreateTokenId("hello"), 1);
  EXPECT_EQ(table.GetTokenId("hello"), 1);
  EXPECT_EQ(table.GetOrCreateTokenId("there"), 2);
  EXPECT_EQ(table.GetTokenId("there"), 2);
  EXPECT_EQ(table.GetTokenId("O'Neill"), -1);
  EXPECT_EQ(table.GetOrCreateTokenId("O'Neill"), 3);
}

TEST_F(TokenTableTest, DeleteToken) {
  TokenTable table(db_.get());
  EXPECT_TRUE(table.Init());

  EXPECT_EQ(table.DeleteToken("hello"), -1);
  EXPECT_EQ(table.GetOrCreateTokenId("hello"), 1);
  EXPECT_EQ(table.DeleteToken("hello"), 1);
}

TEST_F(TokenTableTest, GetToken) {
  TokenTable table(db_.get());
  EXPECT_TRUE(table.Init());

  EXPECT_FALSE(table.GetToken(1).has_value());
  EXPECT_EQ(table.GetOrCreateTokenId("hello"), 1);

  auto token_or = table.GetToken(1);
  EXPECT_TRUE(token_or.has_value());
  EXPECT_EQ(token_or.value(), "hello");
  EXPECT_EQ(table.DeleteToken("hello"), 1);
  EXPECT_FALSE(table.GetToken(1).has_value());
}

TEST_F(TokenTableTest, ChangeToken) {
  TokenTable table(db_.get());
  EXPECT_TRUE(table.Init());

  // Test 1: Cannot change a non-existing token.
  std::string token;
  EXPECT_EQ(table.ChangeToken("foo", "bar"), -1);

  // Test 2: Change an existing token to a unique token.
  EXPECT_EQ(table.GetOrCreateTokenId("foo"), 1);
  EXPECT_EQ(table.ChangeToken("foo", "bar"), 1);

  // Test 3: Change token to itself
  EXPECT_EQ(table.ChangeToken("bar", "bar"), 1);

  // Test 4: It is invalid to change token to be the same as another token
  EXPECT_EQ(table.GetOrCreateTokenId("baz"), 2);
  EXPECT_EQ(table.ChangeToken("bar", "baz"), -1);
}

}  // namespace
}  // namespace ash::file_manager
