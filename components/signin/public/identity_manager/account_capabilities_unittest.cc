// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities.h"

#include "testing/gtest/include/gtest/gtest.h"

class AccountCapabilitiesTest : public testing::Test {};

TEST_F(AccountCapabilitiesTest, CanOfferExtendedChromeSyncPromos) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kUnknown);

  capabilities.set_can_offer_extended_chrome_sync_promos(true);
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kTrue);

  capabilities.set_can_offer_extended_chrome_sync_promos(false);
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Empty) {
  AccountCapabilities capabilities;
  EXPECT_FALSE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Filled) {
  AccountCapabilities capabilities;
  capabilities.set_can_offer_extended_chrome_sync_promos(true);
  EXPECT_TRUE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_UnknownToKnown) {
  AccountCapabilities capabilities;

  AccountCapabilities other;
  other.set_can_offer_extended_chrome_sync_promos(true);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kTrue,
            capabilities.can_offer_extended_chrome_sync_promos());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_KnownToUnknown) {
  AccountCapabilities capabilities;
  capabilities.set_can_offer_extended_chrome_sync_promos(true);

  AccountCapabilities other;

  EXPECT_FALSE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kTrue,
            capabilities.can_offer_extended_chrome_sync_promos());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_OverwriteKnown) {
  AccountCapabilities capabilities;
  capabilities.set_can_offer_extended_chrome_sync_promos(true);

  AccountCapabilities other;
  other.set_can_offer_extended_chrome_sync_promos(false);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kFalse,
            capabilities.can_offer_extended_chrome_sync_promos());
}
