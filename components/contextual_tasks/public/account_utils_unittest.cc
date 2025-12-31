// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/account_utils.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

class AccountUtilsTest : public testing::Test {
 public:
  AccountUtilsTest() = default;
  ~AccountUtilsTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

TEST_F(AccountUtilsTest, GetPrimaryAccountInfoFromProfile_NoPrimaryAccount) {
  CoreAccountInfo account_info = GetPrimaryAccountInfoFromProfile(
      identity_test_environment_.identity_manager());
  EXPECT_TRUE(account_info.IsEmpty());
}

TEST_F(AccountUtilsTest,
       GetPrimaryAccountInfoFromProfile_ReturnsPrimaryAccount) {
  AccountInfo expected_primary_account =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  CoreAccountInfo primary_account = GetPrimaryAccountInfoFromProfile(
      identity_test_environment_.identity_manager());
  EXPECT_EQ(primary_account.account_id, expected_primary_account.account_id);
  EXPECT_EQ(primary_account.gaia, expected_primary_account.gaia);
  EXPECT_EQ(primary_account.email, expected_primary_account.email);
}

TEST_F(AccountUtilsTest, GetAccountFromCookieJar_NoAccounts) {
  std::optional<gaia::ListedAccount> account =
      GetAccountFromCookieJar(identity_test_environment_.identity_manager(),
                              GURL("https://google.com"));
  EXPECT_FALSE(account.has_value());
}

TEST_F(AccountUtilsTest, GetAccountFromCookieJar_NoIndex_ReturnsFirstAccount) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary_account_info =
      identity_test_environment_.MakeAccountAvailable("secondary@example.com");
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia},
       {secondary_account_info.email, secondary_account_info.gaia}});

  std::optional<gaia::ListedAccount> account =
      GetAccountFromCookieJar(identity_test_environment_.identity_manager(),
                              GURL("https://google.com"));
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account->gaia_id, primary_account_info.gaia);
}

TEST_F(AccountUtilsTest,
       GetAccountFromCookieJar_WithIndex_ReturnsCorrectAccount) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary_account_info =
      identity_test_environment_.MakeAccountAvailable("secondary@example.com");
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia},
       {secondary_account_info.email, secondary_account_info.gaia}});

  std::optional<gaia::ListedAccount> account =
      GetAccountFromCookieJar(identity_test_environment_.identity_manager(),
                              GURL("https://google.com?authuser=1"));
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account->gaia_id, secondary_account_info.gaia);
}

TEST_F(AccountUtilsTest, GetAccountFromCookieJar_OutOfBoundsIndex) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary_account_info =
      identity_test_environment_.MakeAccountAvailable("secondary@example.com");
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia},
       {secondary_account_info.email, secondary_account_info.gaia}});

  std::optional<gaia::ListedAccount> account =
      GetAccountFromCookieJar(identity_test_environment_.identity_manager(),
                              GURL("https://google.com?authuser=2"));
  EXPECT_FALSE(account.has_value());
}

TEST_F(AccountUtilsTest, IsUrlForPrimaryAccount_NoPrimaryAccount) {
  // No primary account available.
  EXPECT_FALSE(
      IsUrlForPrimaryAccount(identity_test_environment_.identity_manager(),
                             GURL("https://google.com")));
}

TEST_F(AccountUtilsTest, IsUrlForPrimaryAccount_SingleAccount_Matches) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia}});

  EXPECT_TRUE(
      IsUrlForPrimaryAccount(identity_test_environment_.identity_manager(),
                             GURL("https://google.com")));
}

TEST_F(AccountUtilsTest,
       IsUrlForPrimaryAccount_MultipleAccounts_NoIndex_Ambiguous) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary_account_info =
      identity_test_environment_.MakeAccountAvailable("secondary@example.com");
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia},
       {secondary_account_info.email, secondary_account_info.gaia}});

  // No authuser or /u/ index, so it should be considered for the primary
  // account.
  EXPECT_TRUE(
      IsUrlForPrimaryAccount(identity_test_environment_.identity_manager(),
                             GURL("https://google.com")));
}

TEST_F(AccountUtilsTest,
       IsUrlForPrimaryAccount_MultipleAccounts_Index0_MatchesPrimary) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia},
       {"secondary@example.com",
        signin::GetTestGaiaIdForEmail("secondary@example.com")}});

  EXPECT_TRUE(
      IsUrlForPrimaryAccount(identity_test_environment_.identity_manager(),
                             GURL("https://google.com?authuser=0")));
}

TEST_F(AccountUtilsTest,
       IsUrlForPrimaryAccount_MultipleAccounts_Index1_DoesNotMatchPrimary) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia},
       {"secondary@example.com",
        signin::GetTestGaiaIdForEmail("secondary@example.com")}});

  EXPECT_FALSE(
      IsUrlForPrimaryAccount(identity_test_environment_.identity_manager(),
                             GURL("https://google.com?authuser=1")));
}

TEST_F(AccountUtilsTest,
       IsUrlForPrimaryAccount_MultipleAccounts_OutOfBoundsIndex) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia},
       {"secondary@example.com",
        signin::GetTestGaiaIdForEmail("secondary@example.com")}});

  EXPECT_FALSE(
      IsUrlForPrimaryAccount(identity_test_environment_.identity_manager(),
                             GURL("https://google.com?authuser=2")));
}

TEST_F(AccountUtilsTest,
       IsUrlForPrimaryAccount_MultipleAccounts_PathIndex0_MatchesPrimary) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia},
       {"secondary@example.com",
        signin::GetTestGaiaIdForEmail("secondary@example.com")}});

  EXPECT_TRUE(
      IsUrlForPrimaryAccount(identity_test_environment_.identity_manager(),
                             GURL("https://google.com/u/0/test")));
}

TEST_F(AccountUtilsTest,
       IsUrlForPrimaryAccount_MultipleAccounts_PathIndex1_DoesNotMatchPrimary) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia},
       {"secondary@example.com",
        signin::GetTestGaiaIdForEmail("secondary@example.com")}});

  EXPECT_FALSE(
      IsUrlForPrimaryAccount(identity_test_environment_.identity_manager(),
                             GURL("https://google.com/u/1/test")));
}

TEST_F(AccountUtilsTest, IsUserSignedInToWeb_BrowserAndWebAccounts) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia}});

  EXPECT_TRUE(IsUserSignedInToWeb(identity_test_environment_.identity_manager(),
                                  GURL("https://google.com/u/0/test")));
}

TEST_F(AccountUtilsTest, IsUserSignedInToWeb_WebOnly) {
  identity_test_environment_.SetCookieAccounts(
      {{"primary@example.com",
        signin::GetTestGaiaIdForEmail("primary@example.com")}});

  EXPECT_TRUE(IsUserSignedInToWeb(identity_test_environment_.identity_manager(),
                                  GURL("https://google.com/u/0/test")));
}

TEST_F(AccountUtilsTest, IsUserSignedInToWeb_NoAccounts) {
  EXPECT_FALSE(
      IsUserSignedInToWeb(identity_test_environment_.identity_manager(),
                          GURL("https://google.com/u/0/test")));
}

}  // namespace contextual_tasks
