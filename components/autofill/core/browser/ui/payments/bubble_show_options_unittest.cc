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

  VirtualCardManualFallbackBubbleOptions
  CreateVirtualCardManualFallbackBubbleOptions() {
    VirtualCardManualFallbackBubbleOptions options;
    options.masked_card_name = u"Visa";
    options.masked_card_number_last_four = u"**** 1234";
    options.virtual_card = test::GetVirtualCard();
    options.virtual_card_cvc = u"123";
    options.card_image = gfx::test::CreateImage(40, 24);
    return options;
  }
};

TEST_F(BubbleShowOptionsTest, VirtualCardManualFallbackBubbleOptionsIsValid) {
  // Complete `options` should be valid.
  EXPECT_TRUE(CreateVirtualCardManualFallbackBubbleOptions().IsValid());

  // Missing masked_card_name.
  {
    auto options = CreateVirtualCardManualFallbackBubbleOptions();
    options.masked_card_name = u"";
    EXPECT_FALSE(options.IsValid());
  }

  // Missing masked_card_number_last_four.
  {
    auto options = CreateVirtualCardManualFallbackBubbleOptions();
    options.masked_card_number_last_four = u"";
    EXPECT_FALSE(options.IsValid());
  }

  // Missing virtual card number.
  {
    auto options = CreateVirtualCardManualFallbackBubbleOptions();
    options.virtual_card.SetNumber(u"");
    EXPECT_FALSE(options.IsValid());
  }

  // Missing virtual card cardholder name.
  {
    auto options = CreateVirtualCardManualFallbackBubbleOptions();
    options.virtual_card.SetRawInfo(CREDIT_CARD_NAME_FULL, /*value=*/u"");
    EXPECT_FALSE(options.IsValid());
  }

  // Missing valid virtual card expiration month.
  {
    auto options = CreateVirtualCardManualFallbackBubbleOptions();
    options.virtual_card.SetExpirationMonth(0);
    EXPECT_FALSE(options.IsValid());
  }

  // Missing valid virtual card expiration year.
  {
    auto options = CreateVirtualCardManualFallbackBubbleOptions();
    options.virtual_card.SetExpirationYear(0);
    EXPECT_FALSE(options.IsValid());
  }

  // Missing virtual card CVC.
  {
    auto options = CreateVirtualCardManualFallbackBubbleOptions();
    options.virtual_card_cvc = u"";
    EXPECT_FALSE(options.IsValid());
  }

  // Missing non-empty card art image.
  {
    auto options = CreateVirtualCardManualFallbackBubbleOptions();
    options.card_image = gfx::Image();
    EXPECT_FALSE(options.IsValid());
  }
}

}  // namespace autofill
