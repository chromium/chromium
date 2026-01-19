// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"

#include <cstdint>

#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

namespace {

TEST(LoyaltyCardTest, IsValid) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  EXPECT_TRUE(card.IsValid());

  card.set_id(ValuableId(""));
  EXPECT_FALSE(card.IsValid());
  card.set_id(ValuableId("123"));
  EXPECT_TRUE(card.IsValid());

  card.set_loyalty_card_number("");
  EXPECT_FALSE(card.IsValid());
  card.set_loyalty_card_number("123456");
  EXPECT_TRUE(card.IsValid());

  card.set_merchant_name("");
  EXPECT_FALSE(card.IsValid());
  card.set_merchant_name("Merchant");
  EXPECT_TRUE(card.IsValid());

  card.set_program_logo(GURL("http:://google.com"));
  EXPECT_FALSE(card.IsValid());
}

TEST(LoyaltyCardTest, GetAffiliationCategory) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  card.set_merchant_domains({GURL("https://merchant.com")});

  EXPECT_EQ(card.GetAffiliationCategory(GURL("https://merchant.com")),
            LoyaltyCard::AffiliationCategory::kAffiliated);
  EXPECT_EQ(card.GetAffiliationCategory(GURL("https://sub.merchant.com")),
            LoyaltyCard::AffiliationCategory::kAffiliated);
  EXPECT_EQ(card.GetAffiliationCategory(GURL("https://other.com")),
            LoyaltyCard::AffiliationCategory::kNonAffiliated);
}

TEST(LoyaltyCardTest, Equality) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();

  EXPECT_EQ(card1, test::CreateLoyaltyCard());
  EXPECT_NE(card1, test::CreateLoyaltyCard2());
}

TEST(LoyaltyCardTest, RecordLoyaltyCardUsed) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  int64_t initial_count = card.use_count();

  base::Time time = base::Time::Now();
  card.RecordLoyaltyCardUsed(time);

  EXPECT_EQ(card.use_count(), initial_count + 1);
  EXPECT_EQ(card.use_date(), time);
}

}  // namespace

}  // namespace autofill
