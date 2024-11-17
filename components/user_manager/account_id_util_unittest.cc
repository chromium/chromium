// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/account_id_util.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/values.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_manager {

namespace {
constexpr char kUserEmail[] = "default_account@gmail.com";
constexpr char kOtherEmail[] = "renamed_account@gmail.com";
constexpr char kGaiaID[] = "fake-gaia-id";
// Active directory users are deprecated, but full cleanup is not finished yet.
constexpr char kObjGuid[] = "fake-obj-guid";
}  // namespace

// Base class for tests of known_user.
// Sets up global objects necessary for known_user to be able to access
// local_state.
class AccountIdUtilTest : public testing::Test {
 public:
  AccountIdUtilTest() = default;
  ~AccountIdUtilTest() override = default;

  AccountIdUtilTest(const AccountIdUtilTest& other) = delete;
  AccountIdUtilTest& operator=(const AccountIdUtilTest& other) = delete;

 protected:
  const AccountId kDefaultAccountId =
      AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaID);
};

TEST_F(AccountIdUtilTest, LoadGoogleAccountWithGaiaId) {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("account_type", "google")
                               .Set("email", kUserEmail)
                               .Set("gaia_id", kGaiaID)
                               .Set("obj_guid", kObjGuid);
  std::optional<AccountId> result = LoadAccountId(dict);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_valid());
  EXPECT_EQ(result->GetAccountType(), AccountType::GOOGLE);
  EXPECT_EQ(result->GetUserEmail(), kUserEmail);
  ASSERT_TRUE(result->HasAccountIdKey());
  ASSERT_EQ(result->GetGaiaId(), kGaiaID);
}

TEST_F(AccountIdUtilTest, LoadGoogleAccountWithoutGaiaId) {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("account_type", "google")
                               .Set("email", kUserEmail)
                               .Set("obj_guid", kObjGuid);
  std::optional<AccountId> result = LoadAccountId(dict);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_valid());
  EXPECT_EQ(result->GetAccountType(), AccountType::GOOGLE);
  EXPECT_EQ(result->GetUserEmail(), kUserEmail);
  ASSERT_FALSE(result->HasAccountIdKey());
}

// Death test for some reason fails on MSAN bots.
// TODO(b/325904498): Investigate how DEATH tests should be used.
TEST_F(AccountIdUtilTest, DISABLED_LoadUnknownAccount) {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("account_type", "unknown")
                               .Set("email", kUserEmail)
                               .Set("gaia_id", kGaiaID)
                               .Set("obj_guid", kObjGuid);
  ASSERT_DEATH({ LoadAccountId(dict); }, "Unknown account type");
}

TEST_F(AccountIdUtilTest, LoadAccountEmailOnly) {
  base::Value::Dict dict = base::Value::Dict().Set("email", kUserEmail);
  std::optional<AccountId> result = LoadAccountId(dict);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_valid());
  // Assume that accounts are Google accounts by default.
  EXPECT_EQ(result->GetAccountType(), AccountType::GOOGLE);
  EXPECT_EQ(result->GetUserEmail(), kUserEmail);
  ASSERT_FALSE(result->HasAccountIdKey());
}

TEST_F(AccountIdUtilTest, LoadDeprecatedActiveDirectoryUser) {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("account_type", "ad")
                               .Set("email", kUserEmail)
                               .Set("obj_guid", kObjGuid);
  std::optional<AccountId> result = LoadAccountId(dict);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_valid());
  EXPECT_EQ(result->GetAccountType(), AccountType::ACTIVE_DIRECTORY);
  EXPECT_EQ(result->GetUserEmail(), kUserEmail);
  ASSERT_TRUE(result->HasAccountIdKey());
  EXPECT_EQ(result->GetObjGuid(), kObjGuid);
}

TEST_F(AccountIdUtilTest, MatchByCorrectEmail) {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("account_type", "google")
                               .Set("email", kUserEmail)
                               .Set("gaia_id", kGaiaID);
  AccountId id = AccountId::FromUserEmail(kUserEmail);
  ASSERT_TRUE(AccountIdMatches(id, dict));
}

TEST_F(AccountIdUtilTest, MatchByIncorrectEmail) {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("account_type", "google")
                               .Set("email", kUserEmail)
                               .Set("gaia_id", kGaiaID);
  AccountId id = AccountId::FromUserEmail(kOtherEmail);
  ASSERT_FALSE(AccountIdMatches(id, dict));
}

TEST_F(AccountIdUtilTest, MatchByGaiaIdSameEmail) {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("account_type", "google")
                               .Set("email", kUserEmail)
                               .Set("gaia_id", kGaiaID);
  AccountId id = AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaID);
  ASSERT_TRUE(AccountIdMatches(id, dict));
}

TEST_F(AccountIdUtilTest, MatchByGaiaIdOtherEmail) {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("account_type", "google")
                               .Set("email", kUserEmail)
                               .Set("gaia_id", kGaiaID);
  AccountId id = AccountId::FromUserEmailGaiaId(kOtherEmail, kGaiaID);
  ASSERT_TRUE(AccountIdMatches(id, dict));
}

TEST_F(AccountIdUtilTest, MatchByEmailOnly) {
  base::Value::Dict dict = base::Value::Dict().Set("email", kUserEmail);
  AccountId id = AccountId::FromUserEmail(kUserEmail);
  ASSERT_TRUE(AccountIdMatches(id, dict));
}

TEST_F(AccountIdUtilTest, StoreEmailOnly) {
  AccountId id = AccountId::FromUserEmail(kUserEmail);
  base::Value::Dict dict;
  StoreAccountId(id, dict);
  EXPECT_EQ(dict.Find("account_type"), nullptr);
  EXPECT_EQ(dict.Find("email")->GetString(), kUserEmail);
  EXPECT_EQ(dict.Find("gaia_id"), nullptr);
}

TEST_F(AccountIdUtilTest, StoreGoogleAccount) {
  AccountId id = AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaID);
  base::Value::Dict dict;
  StoreAccountId(id, dict);
  EXPECT_EQ(dict.Find("account_type")->GetString(), "google");
  EXPECT_EQ(dict.Find("email")->GetString(), kUserEmail);
  EXPECT_EQ(dict.Find("gaia_id")->GetString(), kGaiaID);
}

TEST_F(AccountIdUtilTest, StoreDeprecatedADAccount) {
  AccountId id = AccountId::AdFromUserEmailObjGuid(kUserEmail, kObjGuid);
  base::Value::Dict dict;
  StoreAccountId(id, dict);
  EXPECT_EQ(dict.Find("account_type")->GetString(), "ad");
  EXPECT_EQ(dict.Find("email")->GetString(), kUserEmail);
  EXPECT_EQ(dict.Find("obj_guid")->GetString(), kObjGuid);
}

}  // namespace user_manager
