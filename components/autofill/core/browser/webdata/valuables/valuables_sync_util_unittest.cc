// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"

#include "base/types/zip.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

constexpr char kId1[] = "1";
constexpr char kInvalidId[] = "";

constexpr char kValidProgramLogo[] = "http://foobar.com/logo.png";
constexpr char kInvalidProgramLogo[] = "logo.png";
constexpr char kValidCardNumber[] = "80974934820245";

LoyaltyCard TestLoyaltyCard(std::string_view id = kId1) {
  return LoyaltyCard(ValuableId(std::string(id)), "merchant_name",
                     "program_name", GURL("http://foobar.com/logo.png"),
                     kValidCardNumber, {GURL("https://domain.example")});
}

sync_pb::AutofillValuableSpecifics TestLoyaltyCardSpecifics(
    std::string_view id = kId1,
    std::string_view program_logo = kValidProgramLogo,
    std::string_view number = kValidCardNumber) {
  sync_pb::AutofillValuableSpecifics specifics =
      sync_pb::AutofillValuableSpecifics();
  specifics.set_id(std::string(id));

  sync_pb::AutofillValuableSpecifics::LoyaltyCard* loyalty_card =
      specifics.mutable_loyalty_card();
  loyalty_card->set_merchant_name("merchant_name");
  loyalty_card->set_program_name("program_name");
  loyalty_card->set_program_logo(std::string(program_logo));
  loyalty_card->set_loyalty_card_number(number);
  *loyalty_card->add_merchant_domains() = "https://domain.example";
  return specifics;
}

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

TEST_F(LoyaltyCardSyncUtilTest, AreAutofillLoyaltyCardSpecificsValid) {
  EXPECT_FALSE(AreAutofillLoyaltyCardSpecificsValid(
      TestLoyaltyCardSpecifics(kInvalidId)));
  EXPECT_FALSE(AreAutofillLoyaltyCardSpecificsValid(
      TestLoyaltyCardSpecifics(kId1, kInvalidProgramLogo)));
  EXPECT_FALSE(AreAutofillLoyaltyCardSpecificsValid(
      TestLoyaltyCardSpecifics(kId1, kInvalidProgramLogo)));
  EXPECT_FALSE(AreAutofillLoyaltyCardSpecificsValid(
      TestLoyaltyCardSpecifics(kId1, kValidProgramLogo, /*number=*/"")));
  EXPECT_TRUE(
      AreAutofillLoyaltyCardSpecificsValid(TestLoyaltyCardSpecifics(kId1)));

  sync_pb::AutofillValuableSpecifics specifics = TestLoyaltyCardSpecifics(kId1);
  specifics.mutable_loyalty_card()->clear_program_logo();
  EXPECT_TRUE(AreAutofillLoyaltyCardSpecificsValid(specifics));
  EXPECT_TRUE(AreAutofillLoyaltyCardSpecificsValid(
      TestLoyaltyCardSpecifics(kId1, /*program_logo=*/"")));
}

TEST_F(LoyaltyCardSyncUtilTest, TrimAutofillValuableSpecificsDataForCaching) {
  EXPECT_EQ(TrimAutofillValuableSpecificsDataForCaching(
                TestLoyaltyCardSpecifics(kId1))
                .ByteSizeLong(),
            0u);
}

}  // namespace autofill
