// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/loyalty_card_sync_util.h"

#include "components/sync/protocol/autofill_loyalty_card_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

constexpr char kId1[] = "1";
constexpr char kInvalidId[] = "";

constexpr char kValidProgramLogo[] = "http://foobar.com/logo.png";
constexpr char kInvalidProgramLogo[] = "logo.png";

LoyaltyCard TestLoyaltyCard(std::string_view id = kId1) {
  return LoyaltyCard(std::string(id), "merchant_name", "program_name",
                     GURL("http://foobar.com/logo.png"), "suffix");
}

sync_pb::AutofillLoyaltyCardSpecifics TestSpecifics(
    std::string_view id = kId1,
    std::string_view program_logo = kValidProgramLogo) {
  sync_pb::AutofillLoyaltyCardSpecifics specifics =
      sync_pb::AutofillLoyaltyCardSpecifics();
  specifics.set_id(std::string(id));
  specifics.set_merchant_name("merchant_name");
  specifics.set_program_name("program_name");
  specifics.set_program_logo(std::string(program_logo));
  specifics.set_loyalty_card_suffix("suffix");
  return specifics;
}

}  // namespace

class LoyaltyCardSyncUtilTest : public testing::Test {};

TEST_F(LoyaltyCardSyncUtilTest, CreateSpecificsFromLoyaltyCard) {
  LoyaltyCard card = TestLoyaltyCard();
  sync_pb::AutofillLoyaltyCardSpecifics specifics =
      CreateSpecificsFromLoyaltyCard(card);

  EXPECT_EQ(card.id(), specifics.id());
  EXPECT_EQ(card.merchant_name(), specifics.merchant_name());
  EXPECT_EQ(card.program_name(), specifics.program_name());
  EXPECT_EQ(card.program_logo(), specifics.program_logo());
  EXPECT_EQ(card.loyalty_card_suffix(), specifics.loyalty_card_suffix());
}

TEST_F(LoyaltyCardSyncUtilTest, CreateEntityDataFromLoyaltyCard) {
  LoyaltyCard card = TestLoyaltyCard();
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityDataFromLoyaltyCard(card);

  sync_pb::AutofillLoyaltyCardSpecifics specifics =
      entity_data->specifics.autofill_loyalty_card();

  EXPECT_TRUE(entity_data->specifics.has_autofill_loyalty_card());
  EXPECT_EQ(card.id(), specifics.id());
  EXPECT_EQ(card.merchant_name(), specifics.merchant_name());
  EXPECT_EQ(card.program_name(), specifics.program_name());
  EXPECT_EQ(card.program_logo(), specifics.program_logo());
  EXPECT_EQ(card.loyalty_card_suffix(), specifics.loyalty_card_suffix());
}

TEST_F(LoyaltyCardSyncUtilTest, CreateAutofillLoyaltyCardFromSpecifics) {
  EXPECT_EQ(CreateAutofillLoyaltyCardFromSpecifics(TestSpecifics(kInvalidId)),
            std::nullopt);
  EXPECT_EQ(TestLoyaltyCard(),
            CreateAutofillLoyaltyCardFromSpecifics(TestSpecifics(kId1)));
}

TEST_F(LoyaltyCardSyncUtilTest, AreAutofillLoyaltyCardSpecificsValid) {
  EXPECT_FALSE(AreAutofillLoyaltyCardSpecificsValid(TestSpecifics(kInvalidId)));
  EXPECT_FALSE(AreAutofillLoyaltyCardSpecificsValid(
      TestSpecifics(kId1, kInvalidProgramLogo)));
  EXPECT_TRUE(AreAutofillLoyaltyCardSpecificsValid(TestSpecifics(kId1)));
}

TEST_F(LoyaltyCardSyncUtilTest, TrimLoyaltyCardSpecificsDataForCaching) {
  EXPECT_EQ(TrimLoyaltyCardSpecificsDataForCaching(TestSpecifics(kId1))
                .ByteSizeLong(),
            0u);
}

}  // namespace autofill
