// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"

#include <vector>

#include "components/prefs/pref_registry_simple.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DiceTestSigninClient : public testing::StrictMock<TestSigninClient> {
 public:
  DiceTestSigninClient(PrefService* prefs)
      : testing::StrictMock<TestSigninClient>(prefs) {}

  MOCK_METHOD1(SetReadyForDiceMigration, void(bool is_ready));
};

}  // namespace

namespace signin {

TEST(DiceAccountReconcilorDelegateTest, RevokeTokens) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  TestSigninClient client(&pref_service);
  {
    // Dice is enabled, revoke only tokens in error state.
    DiceAccountReconcilorDelegate delegate(&client,
                                           AccountConsistencyMethod::kDice);
    EXPECT_EQ(
        signin::AccountReconcilorDelegate::RevokeTokenOption::kRevokeIfInError,
        delegate.ShouldRevokeSecondaryTokensBeforeReconcile(
            std::vector<gaia::ListedAccount>()));
  }
  {
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDiceMigration);
    // Gaia accounts are not empty, don't revoke.
    gaia::ListedAccount gaia_account;
    gaia_account.id = "other";
    EXPECT_EQ(
        signin::AccountReconcilorDelegate::RevokeTokenOption::kDoNotRevoke,
        delegate.ShouldRevokeSecondaryTokensBeforeReconcile(
            std::vector<gaia::ListedAccount>{gaia_account}));
    // Revoke.
    EXPECT_EQ(signin::AccountReconcilorDelegate::RevokeTokenOption::kRevoke,
              delegate.ShouldRevokeSecondaryTokensBeforeReconcile(
                  std::vector<gaia::ListedAccount>()));
  }
}

TEST(DiceAccountReconcilorDelegateTest, OnReconcileFinished) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  pref_service.registry()->RegisterBooleanPref(
      prefs::kTokenServiceDiceCompatible, false);

  DiceTestSigninClient client(&pref_service);

  {
    // Dice migration not enabled.
    testing::InSequence mock_sequence;
    EXPECT_CALL(client, SetReadyForDiceMigration(testing::_)).Times(0);
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDiceFixAuthErrors);
    delegate.OnReconcileFinished("account", true /* is_reconcile_noop */);
  }

  {
    // Dice migration enabled, but token service is not ready.
    testing::InSequence mock_sequence;
    EXPECT_CALL(client, SetReadyForDiceMigration(false)).Times(1);
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDiceMigration);
    delegate.OnReconcileFinished("account", true /* is_reconcile_noop */);
  }

  pref_service.SetBoolean(prefs::kTokenServiceDiceCompatible, true);

  {
    // Dice migration enabled, token service is ready, but reconcile is not
    // no-op.
    testing::InSequence mock_sequence;
    EXPECT_CALL(client, SetReadyForDiceMigration(false)).Times(1);
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDiceMigration);
    delegate.OnReconcileFinished("account", false /* is_reconcile_noop */);
  }

  {
    // Ready for migration.
    testing::InSequence mock_sequence;
    EXPECT_CALL(client, SetReadyForDiceMigration(true)).Times(1);
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDiceMigration);
    delegate.OnReconcileFinished("account", true /* is_reconcile_noop */);
  }
}

TEST(DiceAccountReconcilorDelegateTest, ShouldRevokeTokensOnCookieDeleted) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  TestSigninClient client(&pref_service);
  {
    // Dice is enabled, revoke tokens when Gaia cookie is deleted.
    DiceAccountReconcilorDelegate delegate(&client,
                                           AccountConsistencyMethod::kDice);
    EXPECT_EQ(true, delegate.ShouldRevokeTokensOnCookieDeleted());
  }
  {
    // Dice is not enabled, do not revoke tokens when Gaia cookie is deleted.
    DiceAccountReconcilorDelegate delegate(
        &client, AccountConsistencyMethod::kDiceMigration);
    EXPECT_EQ(false, delegate.ShouldRevokeTokensOnCookieDeleted());
  }
}

}  // namespace signin
