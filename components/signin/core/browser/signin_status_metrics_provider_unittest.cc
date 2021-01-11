// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_status_metrics_provider.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

using signin::PrimaryAccountChangeEvent;

CoreAccountInfo TestAccount() {
  CoreAccountInfo account;
  account.email = "test@gmail.com";
  account.gaia = signin::GetTestGaiaIdForEmail(account.email);
  account.account_id = CoreAccountId::FromEmail(account.email);
  return account;
}

TEST(SigninStatusMetricsProviderTest, UpdateInitialSigninStatus) {
  SigninStatusMetricsProvider metrics_provider(nullptr, true);

  metrics_provider.UpdateInitialSigninStatus(2, 2);
  EXPECT_EQ(SigninStatusMetricsProviderBase::ALL_PROFILES_SIGNED_IN,
            metrics_provider.GetSigninStatusForTesting());
  metrics_provider.UpdateInitialSigninStatus(2, 0);
  EXPECT_EQ(SigninStatusMetricsProviderBase::ALL_PROFILES_NOT_SIGNED_IN,
            metrics_provider.GetSigninStatusForTesting());
  metrics_provider.UpdateInitialSigninStatus(2, 1);
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());
}

TEST(SigninStatusMetricsProviderTest, OnPrimaryAccountSet) {
  SigninStatusMetricsProvider metrics_provider(nullptr, true);
  CoreAccountInfo account_info = TestAccount();

  // Initial status is all signed out and then one of the profiles is signed in.
  metrics_provider.UpdateInitialSigninStatus(2, 0);
  metrics_provider.OnPrimaryAccountChanged(PrimaryAccountChangeEvent(
      PrimaryAccountChangeEvent::State(),
      PrimaryAccountChangeEvent::State(account_info,
                                       signin::ConsentLevel::kSync)));
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());

  // Initial status is mixed and then one of the profiles is signed in.
  metrics_provider.UpdateInitialSigninStatus(2, 1);
  metrics_provider.OnPrimaryAccountChanged(PrimaryAccountChangeEvent(
      PrimaryAccountChangeEvent::State(),
      PrimaryAccountChangeEvent::State(account_info,
                                       signin::ConsentLevel::kSync)));
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());
}

TEST(SigninStatusMetricsProviderTest, OnPrimaryAccountCleared) {
  SigninStatusMetricsProvider metrics_provider(nullptr, true);
  CoreAccountInfo account_info = TestAccount();

  // Initial status is all signed in and then one of the profiles is signed out.
  metrics_provider.UpdateInitialSigninStatus(2, 2);
  metrics_provider.OnPrimaryAccountChanged(
      PrimaryAccountChangeEvent(PrimaryAccountChangeEvent::State(
                                    account_info, signin::ConsentLevel::kSync),
                                PrimaryAccountChangeEvent::State()));
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());

  // Initial status is mixed and then one of the profiles is signed out.
  metrics_provider.UpdateInitialSigninStatus(2, 1);
  metrics_provider.OnPrimaryAccountChanged(
      PrimaryAccountChangeEvent(PrimaryAccountChangeEvent::State(
                                    account_info, signin::ConsentLevel::kSync),
                                PrimaryAccountChangeEvent::State()));
  EXPECT_EQ(SigninStatusMetricsProviderBase::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());
}
