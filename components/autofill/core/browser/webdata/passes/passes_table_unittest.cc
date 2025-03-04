// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/passes/passes_table.h"

#include "base/files/scoped_temp_dir.h"
#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"
#include "components/autofill/core/browser/test_utils/passes_data_test_utils.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class PassesTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    ASSERT_EQ(sql::INIT_OK,
              db_.Init(temp_dir_.GetPath().AppendASCII("TestDB")));
  }

  PassesTable& passes_table() { return table_; }

 private:
  base::ScopedTempDir temp_dir_;
  PassesTable table_;
  WebDatabase db_;
};

TEST_F(PassesTableTest, GetLoyaltyCards) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  ASSERT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card1));
  ASSERT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card2));
  EXPECT_THAT(passes_table().GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
}

TEST_F(PassesTableTest, GetLoyaltyCardById) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  ASSERT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card1));
  ASSERT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card2));
  EXPECT_EQ(passes_table().GetLoyaltyCardById(card1.loyalty_card_id), card1);
  EXPECT_EQ(passes_table().GetLoyaltyCardById(card2.loyalty_card_id), card2);
  EXPECT_EQ(passes_table().GetLoyaltyCardById("invalid_id"), std::nullopt);
}

TEST_F(PassesTableTest, AddOrUpdateLoyaltyCard) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  LoyaltyCard card2 = test::CreateLoyaltyCard2();
  // Add `card1`.
  EXPECT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card1));
  EXPECT_THAT(passes_table().GetLoyaltyCards(), UnorderedElementsAre(card1));
  // Update `card1`.
  card1.unmasked_loyalty_card_suffix = "9876";
  EXPECT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card1));
  EXPECT_THAT(passes_table().GetLoyaltyCards(), UnorderedElementsAre(card1));
  // Add `card2`.
  EXPECT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card2));
  EXPECT_THAT(passes_table().GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
}

TEST_F(PassesTableTest, AddOrUpdateLoyaltyCard_EmptyProgramLogoUrl) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  card1.program_logo = GURL("");
  EXPECT_TRUE(card1.program_logo.is_empty());
  EXPECT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card1));
  EXPECT_THAT(passes_table().GetLoyaltyCards(), UnorderedElementsAre(card1));
}

TEST_F(PassesTableTest, AddOrUpdateLoyaltyCard_InvalidProgramLogoUrl) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  card1.program_logo = GURL("http:://google.com");

  EXPECT_FALSE(card1.program_logo.is_empty());
  EXPECT_FALSE(passes_table().AddOrUpdateLoyaltyCard(card1));
  EXPECT_THAT(passes_table().GetLoyaltyCards(), IsEmpty());
}

TEST_F(PassesTableTest, AddOrUpdateLoyaltyCard_EmptyLoyaltyCardId) {
  LoyaltyCard card1 = test::CreateLoyaltyCard();
  card1.loyalty_card_id = "";

  EXPECT_FALSE(passes_table().AddOrUpdateLoyaltyCard(card1));
  EXPECT_THAT(passes_table().GetLoyaltyCards(), IsEmpty());
}

TEST_F(PassesTableTest, RemoveLoyaltyCard) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  ASSERT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card1));
  ASSERT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card2));
  EXPECT_THAT(passes_table().GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
  EXPECT_TRUE(passes_table().RemoveLoyaltyCard(card1.loyalty_card_id));
  EXPECT_THAT(passes_table().GetLoyaltyCards(), UnorderedElementsAre(card2));
  // Removing a non-existing `loyalty_card_id` shouldn't be considered a
  // failure.
  EXPECT_TRUE(passes_table().RemoveLoyaltyCard(card1.loyalty_card_id));
}

TEST_F(PassesTableTest, ClearLoyaltyCards) {
  const LoyaltyCard card1 = test::CreateLoyaltyCard();
  const LoyaltyCard card2 = test::CreateLoyaltyCard2();
  ASSERT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card1));
  ASSERT_TRUE(passes_table().AddOrUpdateLoyaltyCard(card2));
  EXPECT_THAT(passes_table().GetLoyaltyCards(),
              UnorderedElementsAre(card1, card2));
  EXPECT_TRUE(passes_table().ClearLoyaltyCards());
  EXPECT_THAT(passes_table().GetLoyaltyCards(), IsEmpty());
}

}  // namespace

}  // namespace autofill
