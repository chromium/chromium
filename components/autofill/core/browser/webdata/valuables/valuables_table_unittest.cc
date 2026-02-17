// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"

#include "base/files/scoped_temp_dir.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class ValuablesTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    ASSERT_EQ(sql::INIT_OK,
              db_.Init(temp_dir_.GetPath().AppendASCII("TestDB")));
  }

  ValuablesTable& valuables_table() { return table_; }

 private:
  base::ScopedTempDir temp_dir_;
  ValuablesTable table_;
  WebDatabase db_;
};

TEST_F(ValuablesTableTest, GetLoyaltyCards) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  ASSERT_TRUE(valuables_table().SetLoyaltyCards({card1, card2}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
}

TEST_F(ValuablesTableTest, GetLoyaltyCardById) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  ASSERT_TRUE(valuables_table().SetLoyaltyCards({card1, card2}));
  EXPECT_EQ(valuables_table().GetLoyaltyCardById(card1.id()), card1);
  EXPECT_EQ(valuables_table().GetLoyaltyCardById(card2.id()), card2);
  EXPECT_EQ(valuables_table().GetLoyaltyCardById(ValuableId("invalid_id")),
            std::nullopt);
}

TEST_F(ValuablesTableTest, AddOrUpdateLoyaltyCard) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  LoyaltyCard card2 = test::CreateLoyaltyCard2();
  // Add `card1`.
  EXPECT_TRUE(valuables_table().SetLoyaltyCards({card1}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), UnorderedElementsAre(card1));
  // Update `card1`.
  card1.set_loyalty_card_number("9876");
  EXPECT_TRUE(valuables_table().SetLoyaltyCards({card1}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), UnorderedElementsAre(card1));
  // Add `card2`.
  EXPECT_TRUE(valuables_table().SetLoyaltyCards({card1, card2}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
}

TEST_F(ValuablesTableTest, AddOrUpdateLoyaltyCard_EmptyProgramLogoUrl) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  card1.set_program_logo(GURL::EmptyGURL());
  EXPECT_TRUE(card1.program_logo().is_empty());
  EXPECT_TRUE(valuables_table().SetLoyaltyCards({card1}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), UnorderedElementsAre(card1));
}

TEST_F(ValuablesTableTest, AddOrUpdateLoyaltyCard_InvalidProgramLogoUrl) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  card1.set_program_logo(GURL("http:://google.com"));

  EXPECT_FALSE(card1.program_logo().is_empty());
  EXPECT_FALSE(valuables_table().SetLoyaltyCards({card1}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), IsEmpty());
}

TEST_F(ValuablesTableTest, AddOrUpdateLoyaltyCard_EmptyLoyaltyCardId) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  card1.set_id(ValuableId(""));

  EXPECT_FALSE(valuables_table().SetLoyaltyCards({card1}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), IsEmpty());
}

TEST_F(ValuablesTableTest, AddOrUpdateLoyaltyCard_EmptyLoyaltyCardNumber) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  card1.set_loyalty_card_number("");

  EXPECT_FALSE(valuables_table().SetLoyaltyCards({card1}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), IsEmpty());
}

TEST_F(ValuablesTableTest, AddOrUpdateLoyaltyCard_EmptyMerchantName) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  card1.set_merchant_name("");

  EXPECT_FALSE(valuables_table().SetLoyaltyCards({card1}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), IsEmpty());
}

TEST_F(ValuablesTableTest, RemoveLoyaltyCard) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  ASSERT_TRUE(valuables_table().SetLoyaltyCards({card1, card2}));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
  EXPECT_TRUE(valuables_table().RemoveLoyaltyCard(card1.id()));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), UnorderedElementsAre(card2));
  // Removing a non-existing `id()` shouldn't be considered a
  // failure.
  EXPECT_TRUE(valuables_table().RemoveLoyaltyCard(card1.id()));
}

TEST_F(ValuablesTableTest, AddOrUpdateLoyaltyCard_AddNew) {
  const LoyaltyCard card = test::CreateLoyaltyCard();
  EXPECT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), UnorderedElementsAre(card));
}

TEST_F(ValuablesTableTest, AddOrUpdateLoyaltyCard_Update) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  valuables_table().AddOrUpdateLoyaltyCard(card);
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), UnorderedElementsAre(card));

  card.set_program_name("new program name");
  card.set_merchant_domains({GURL("https://new.merchant.com")});
  EXPECT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), UnorderedElementsAre(card));
}

TEST_F(ValuablesTableTest, AddOrUpdateLoyaltyCard_Invalid) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  card.set_merchant_name("");
  EXPECT_FALSE(valuables_table().AddOrUpdateLoyaltyCard(card));
  EXPECT_THAT(valuables_table().GetLoyaltyCards(), IsEmpty());
}

TEST_F(ValuablesTableTest, GetLoyaltyCards_MissingMetadata) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  ASSERT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card));
  ASSERT_TRUE(valuables_table().RemoveValuableMetadata(card.id()));

  std::vector<LoyaltyCard> cards = valuables_table().GetLoyaltyCards();
  ASSERT_THAT(cards, testing::SizeIs(1));
  EXPECT_EQ(cards[0].metadata(), ValuableMetadata(card.id(), base::Time(), 0u));
}

TEST_F(ValuablesTableTest, RemoveLoyaltyCardRemovesMetadata) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  ASSERT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card));
  ASSERT_TRUE(valuables_table().GetValuableMetadata(card.id()).has_value());

  EXPECT_TRUE(valuables_table().RemoveLoyaltyCard(card.id()));
  EXPECT_FALSE(valuables_table().GetValuableMetadata(card.id()).has_value());
}

TEST_F(ValuablesTableTest, AddOrUpdateValuableMetadata_Add) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  ASSERT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card));

  ValuableMetadata metadata = card.metadata();
  metadata.use_count = 10;
  metadata.use_date = base::Time::FromTimeT(100);

  EXPECT_TRUE(valuables_table().AddOrUpdateValuableMetadata(metadata));
  EXPECT_EQ(valuables_table().GetValuableMetadata(card.id()), metadata);
}

TEST_F(ValuablesTableTest, AddOrUpdateValuableMetadata_Update) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  ASSERT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card));

  ValuableMetadata metadata = card.metadata();
  metadata.use_count = 1;
  EXPECT_TRUE(valuables_table().AddOrUpdateValuableMetadata(metadata));
  EXPECT_EQ(valuables_table().GetValuableMetadata(card.id())->use_count, 1u);
}

TEST_F(ValuablesTableTest, RemoveValuableMetadata) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  ASSERT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card));

  EXPECT_TRUE(valuables_table().RemoveValuableMetadata(card.id()));
  EXPECT_FALSE(valuables_table().GetValuableMetadata(card.id()).has_value());
}

TEST_F(ValuablesTableTest, GetValuableMetadata_ExistingEntity) {
  LoyaltyCard card = test::CreateLoyaltyCard();
  ASSERT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card));

  std::optional<ValuableMetadata> metadata =
      valuables_table().GetValuableMetadata(card.id());
  EXPECT_THAT(metadata, testing::Optional(card.metadata()));
}

TEST_F(ValuablesTableTest, GetValuableMetadata_NonExistentEntity) {
  std::optional<ValuableMetadata> metadata =
      valuables_table().GetValuableMetadata(ValuableId("non-existent-id"));
  EXPECT_FALSE(metadata.has_value());
}

TEST_F(ValuablesTableTest, GetAllValuableMetadata) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  LoyaltyCard card2 = test::CreateLoyaltyCard2();

  ASSERT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card1));
  ASSERT_TRUE(valuables_table().AddOrUpdateLoyaltyCard(card2));

  EXPECT_THAT(valuables_table().GetAllValuableMetadata(),
              UnorderedElementsAre(Pair(card1.id(), card1.metadata()),
                                   Pair(card2.id(), card2.metadata())));
}

}  // namespace

}  // namespace autofill
