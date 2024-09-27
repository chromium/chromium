// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/test_accounts.h"

#include <string>
#include <string_view>

#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::FilePath;

namespace signin::test {
namespace {

class TestAccountsTest : public testing::Test {};

FilePath WriteContentToTemporaryFile(std::string_view contents) {
  FilePath tmp_file;
  CHECK(base::CreateTemporaryFile(&tmp_file));
  bool success = base::WriteFile(tmp_file, contents);
  CHECK(success);
  return tmp_file;
}

TEST(TestAccountsTest, ParsingJson) {
  std::string contents = R"json(
      {
        "TEST_ACCOUNT_1": {
          "win": {
            "user": "user1",
            "password": "pwd1"
          }
        }
      }
  )json";
  TestAccountsConfig test_accounts_config;
  ASSERT_TRUE(test_accounts_config.Init(WriteContentToTemporaryFile(contents)));
}

TEST(TestAccountsTest, GetAccountForPlatformSpecific) {
  std::string contents = R"json(
      {
        "TEST_ACCOUNT_1": {
          "win": {
            "user": "user1",
            "password": "pwd1"
          },
          "mac": {
            "user": "user1",
            "password": "pwd1"
          },
          "linux": {
            "user": "user1",
            "password": "pwd1"
          },
          "chromeos": {
            "user": "user1",
            "password": "pwd1"
          },
          "android": {
            "user": "user1",
            "password": "pwd1"
          },
          "fuchsia": {
            "user": "user1",
            "password": "pwd1"
          },
          "ios": {
            "user": "user1",
            "password": "pwd1"
          }
        }
      }
  )json";

  FilePath tmp_file = WriteContentToTemporaryFile(contents);
  TestAccountsConfig test_accounts_config;
  ASSERT_TRUE(test_accounts_config.Init(tmp_file));
  std::optional<TestAccountSigninCredentials> credentials =
      test_accounts_config.GetAccount("TEST_ACCOUNT_1");
  ASSERT_TRUE(credentials.has_value());
  EXPECT_EQ(credentials->user, "user1");
  EXPECT_EQ(credentials->password, "pwd1");
}

TEST(TestAccountsTest, GetAccountForAllPlatform) {
  std::string contents = R"json(
      {
        "TEST_ACCOUNT_1": {
          "all_platform": {
            "user": "user_allplatform",
            "password": "pwd_allplatform"
          }
        }
      }
  )json";

  FilePath tmp_file = WriteContentToTemporaryFile(contents);
  TestAccountsConfig test_accounts_config;
  ASSERT_TRUE(test_accounts_config.Init(tmp_file));
  std::optional<TestAccountSigninCredentials> credentials =
      test_accounts_config.GetAccount("TEST_ACCOUNT_1");
  ASSERT_TRUE(credentials.has_value());
  EXPECT_EQ(credentials->user, "user_allplatform");
  EXPECT_EQ(credentials->password, "pwd_allplatform");
}

}  // namespace
}  // namespace signin::test
