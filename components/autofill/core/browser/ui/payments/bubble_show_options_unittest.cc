// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

class BubbleShowOptionsTest : public testing::Test {
 public:
  BubbleShowOptionsTest() = default;
  ~BubbleShowOptionsTest() override = default;

  FilledCardInformationBubbleOptions
  CreateFilledCardInformationBubbleOptions() {
    FilledCardInformationBubbleOptions options;
    options.masked_card_name = u"Visa";
    options.masked_card_number_last_four = u"**** 1234";
    options.filled_card = test::GetVirtualCard();
    options.cvc = u"123";
    options.card_image = gfx::test::CreateImage(40, 24);
    return options;
  }
};

TEST_F(BubbleShowOptionsTest, FilledCardInformationBubbleOptionsIsValid) {
  // Complete `options` should be valid.
  EXPECT_TRUE(CreateFilledCardInformationBubbleOptions().IsValid());

  // Missing masked_card_name.
  {
    auto options = CreateFilledCardInformationBubbleOptions();
    options.masked_card_name = u"";
    EXPECT_FALSE(options.IsValid());
  }

  // Missing masked_card_number_last_four.
  {
    auto options = CreateFilledCardInformationBubbleOptions();
    options.masked_card_number_last_four = u"";
    EXPECT_FALSE(options.IsValid());
  }

  // Missing virtual card number.
  {
    auto options = CreateFilledCardInformationBubbleOptions();
    options.filled_card.SetNumber(u"");
    EXPECT_FALSE(options.IsValid());
  }

  // Missing virtual card cardholder name.
  {
    auto options = CreateFilledCardInformationBubbleOptions();
    options.filled_card.SetRawInfo(CREDIT_CARD_NAME_FULL, /*value=*/u"");
    EXPECT_FALSE(options.IsValid());
  }

  // Missing valid virtual card expiration month.
  {
    auto options = CreateFilledCardInformationBubbleOptions();
    options.filled_card.SetExpirationMonth(0);
    EXPECT_FALSE(options.IsValid());
  }

  // Missing valid virtual card expiration year.
  {
    auto options = CreateFilledCardInformationBubbleOptions();
    options.filled_card.SetExpirationYear(0);
    EXPECT_FALSE(options.IsValid());
  }

  // Missing virtual card CVC.
  {
    auto options = CreateFilledCardInformationBubbleOptions();
    options.cvc = u"";
    EXPECT_FALSE(options.IsValid());
  }

  // Missing non-empty card art image.
  {
    auto options = CreateFilledCardInformationBubbleOptions();
    options.card_image = gfx::Image();
    EXPECT_FALSE(options.IsValid());
  }
}

}  // namespace autofill
