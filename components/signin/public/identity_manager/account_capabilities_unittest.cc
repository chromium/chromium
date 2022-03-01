// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"

#include "testing/gtest/include/gtest/gtest.h"

class AccountCapabilitiesTest : public testing::Test {};

TEST_F(AccountCapabilitiesTest, CanOfferExtendedChromeSyncPromos) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kTrue);

  mutator.set_can_offer_extended_chrome_sync_promos(false);
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanRunChromePrivacySandboxTrials) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kTrue);

  mutator.set_can_offer_extended_chrome_sync_promos(false);
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Empty) {
  AccountCapabilities capabilities;
  EXPECT_FALSE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Filled) {
  AccountCapabilities capabilities;

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);
  EXPECT_TRUE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_UnknownToKnown) {
  AccountCapabilities capabilities;

  AccountCapabilities other;
  AccountCapabilitiesTestMutator mutator(&other);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kTrue,
            capabilities.can_offer_extended_chrome_sync_promos());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_KnownToUnknown) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  AccountCapabilities other;

  EXPECT_FALSE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kTrue,
            capabilities.can_offer_extended_chrome_sync_promos());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_OverwriteKnown) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  AccountCapabilities other;
  AccountCapabilitiesTestMutator other_mutator(&other);
  other_mutator.set_can_offer_extended_chrome_sync_promos(false);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kFalse,
            capabilities.can_offer_extended_chrome_sync_promos());
}
