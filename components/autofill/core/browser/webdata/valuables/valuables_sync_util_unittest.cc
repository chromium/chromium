// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"

#include "base/types/zip.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_test_utils.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
constexpr char kId1[] = "1";
}  // namespace

class LoyaltyCardSyncUtilTest : public testing::Test {};

TEST_F(LoyaltyCardSyncUtilTest, CreateValuableSpecificsFromLoyaltyCard) {
  LoyaltyCard card = TestLoyaltyCard();
  sync_pb::AutofillValuableSpecifics specifics =
      CreateSpecificsFromLoyaltyCard(card);

  EXPECT_EQ(card.id().value(), specifics.id());
  EXPECT_EQ(card.merchant_name(), specifics.loyalty_card().merchant_name());
  EXPECT_EQ(card.program_name(), specifics.loyalty_card().program_name());
  EXPECT_EQ(card.program_logo(), specifics.loyalty_card().program_logo());
  EXPECT_EQ(card.loyalty_card_number(),
            specifics.loyalty_card().loyalty_card_number());
  ASSERT_EQ(card.merchant_domains().size(),
            (size_t)specifics.loyalty_card().merchant_domains().size());
  for (size_t i = 0; i < card.merchant_domains().size(); i++) {
    EXPECT_EQ(card.merchant_domains()[i],
              specifics.loyalty_card().merchant_domains(i));
  }
}

TEST_F(LoyaltyCardSyncUtilTest, CreateEntityDataFromLoyaltyCard) {
  LoyaltyCard card = TestLoyaltyCard();
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromLoyaltyCard(card);

  sync_pb::AutofillValuableSpecifics specifics =
      entity_data->specifics.autofill_valuable();

  EXPECT_TRUE(entity_data->specifics.has_autofill_valuable());
  EXPECT_EQ(card.id().value(), specifics.id());
  EXPECT_EQ(card.merchant_name(), specifics.loyalty_card().merchant_name());
  EXPECT_EQ(card.program_name(), specifics.loyalty_card().program_name());
  EXPECT_EQ(card.program_logo(), specifics.loyalty_card().program_logo());
  EXPECT_EQ(card.loyalty_card_number(),
            specifics.loyalty_card().loyalty_card_number());
  ASSERT_EQ(card.merchant_domains().size(),
            (size_t)specifics.loyalty_card().merchant_domains().size());
  for (auto [merchant_domain, loyalty_card_domain] :
       base::zip(card.merchant_domains(),
                 specifics.loyalty_card().merchant_domains())) {
    EXPECT_EQ(merchant_domain, loyalty_card_domain);
  }
}

TEST_F(LoyaltyCardSyncUtilTest, CreateAutofillLoyaltyCardFromSpecifics) {
  EXPECT_EQ(TestLoyaltyCard(), CreateAutofillLoyaltyCardFromSpecifics(
                                   TestLoyaltyCardSpecifics(kId1)));
}

TEST_F(LoyaltyCardSyncUtilTest, TrimAutofillValuableSpecificsDataForCaching) {
  EXPECT_EQ(TrimAutofillValuableSpecificsDataForCaching(
                TestLoyaltyCardSpecifics(kId1))
                .ByteSizeLong(),
            0u);
}

}  // namespace autofill
