// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

class AccountsInCookieJarInfoTest : public testing::Test {
 public:
  AccountsInCookieJarInfoTest() {
    valid_account1_ = CreateListedAccount("a@example.com", "gaia_a");
    valid_account1_ = CreateListedAccount("b@example.com", "gaia_b");

    invalid_account_ = CreateListedAccount("c@example.com", "gaia_b");
    invalid_account_.valid = false;

    signed_out_account_ = CreateListedAccount("d@example.com", "gaia_c");
    signed_out_account_.signed_out = true;
  }

 protected:
  gaia::ListedAccount valid_account1_;
  gaia::ListedAccount valid_account2_;
  gaia::ListedAccount invalid_account_;
  gaia::ListedAccount signed_out_account_;

 private:
  static gaia::ListedAccount CreateListedAccount(const std::string& email,
                                                 const std::string& gaia_id) {
    gaia::ListedAccount result;
    result.id = CoreAccountId::FromGaiaId(gaia_id);
    result.email = email;
    result.gaia_id = gaia_id;
    result.raw_email = email;
    return result;
  }
};

TEST_F(AccountsInCookieJarInfoTest, InvalidSignedInAccountsFiltered) {
  signin::AccountsInCookieJarInfo cookies(
      false, {valid_account1_, invalid_account_, valid_account2_},
      {signed_out_account_});
  EXPECT_THAT(cookies.GetValidSignedInAccounts(),
              ElementsAre(valid_account1_, valid_account2_));
  EXPECT_THAT(cookies.GetPotentiallyInvalidSignedInAccounts(),
              ElementsAre(valid_account1_, invalid_account_, valid_account2_));
  EXPECT_THAT(cookies.GetSignedOutAccounts(), ElementsAre(signed_out_account_));
}
