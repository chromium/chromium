// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mirror_landing_account_reconcilor_delegate.h"

#include <string>

#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

gaia::ListedAccount BuildListedAccount(const std::string& gaia_id) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(gaia_id);
  gaia::ListedAccount gaia_account;
  gaia_account.id = account_id;
  gaia_account.email = gaia_id + std::string("@gmail.com");
  gaia_account.gaia_id = gaia_id;
  gaia_account.raw_email = gaia_account.email;
  return gaia_account;
}

}  // namespace

TEST(MirrorLandingAccountReconcilorDelegateTest,
     GetChromeAccountsForReconcile) {
  CoreAccountId kPrimaryAccountId = CoreAccountId::FromGaiaId("primary");
  CoreAccountId kOtherAccountId1 = CoreAccountId::FromGaiaId("1");
  CoreAccountId kOtherAccountId2 = CoreAccountId::FromGaiaId("2");
  gaia::ListedAccount gaia_account_primary = BuildListedAccount("primary");
  gaia::ListedAccount gaia_account_1 = BuildListedAccount("1");
  gaia::ListedAccount gaia_account_2 = BuildListedAccount("2");
  gaia::ListedAccount gaia_account_3 = BuildListedAccount("3");
  MirrorLandingAccountReconcilorDelegate delegate;
  // No primary account. Gaia accounts are removed.
  EXPECT_TRUE(
      delegate
          .GetChromeAccountsForReconcile(
              /*chrome_accounts=*/{},
              /*primary_account=*/CoreAccountId(),
              /*gaia_accounts=*/{gaia_account_1, gaia_account_2},
              /*first_execution=*/true,
              /*primary_has_error=*/false,
              gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER)
          .empty());
  // With primary account. Primary is moved in front, account 1 is kept in the
  // same slot, account 2 is added, account 3 is removed.
  EXPECT_EQ(delegate.GetChromeAccountsForReconcile(
                /*chrome_accounts=*/{kOtherAccountId1, kOtherAccountId2,
                                     kPrimaryAccountId},
                /*primary_account=*/kPrimaryAccountId,
                /*gaia_accounts=*/
                {gaia_account_3, gaia_account_primary, gaia_account_1},
                /*first_execution=*/true,
                /*primary_has_error=*/false,
                gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER),
            (std::vector<CoreAccountId>{kPrimaryAccountId, kOtherAccountId2,
                                        kOtherAccountId1}));
  // Primary account error causes a logout.
  EXPECT_TRUE(
      delegate
          .GetChromeAccountsForReconcile(
              /*chrome_accounts=*/{kPrimaryAccountId, kOtherAccountId1},
              /*primary_account=*/kPrimaryAccountId,
              /*gaia_accounts=*/{gaia_account_primary, gaia_account_1},
              /*first_execution=*/true,
              /*primary_has_error=*/true,
              gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER)
          .empty());
}

}  // namespace signin
