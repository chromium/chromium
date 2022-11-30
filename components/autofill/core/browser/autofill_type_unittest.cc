// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

TEST(AutofillTypeTest, ServerFieldTypes) {
  // No server data.
  AutofillType none(NO_SERVER_DATA);
  EXPECT_EQ(NO_SERVER_DATA, none.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kNoGroup, none.group());

  // Unknown type.
  AutofillType unknown(UNKNOWN_TYPE);
  EXPECT_EQ(UNKNOWN_TYPE, unknown.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kNoGroup, unknown.group());

  // Type with group but no subgroup.
  AutofillType first(NAME_FIRST);
  EXPECT_EQ(NAME_FIRST, first.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kName, first.group());

  // Type with group and subgroup.
  AutofillType phone(PHONE_HOME_NUMBER);
  EXPECT_EQ(PHONE_HOME_NUMBER, phone.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kPhoneHome, phone.group());

  // Boundary (error) condition.
  AutofillType boundary(MAX_VALID_FIELD_TYPE);
  EXPECT_EQ(UNKNOWN_TYPE, boundary.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kNoGroup, boundary.group());

  // Beyond the boundary (error) condition.
  AutofillType beyond(static_cast<ServerFieldType>(MAX_VALID_FIELD_TYPE + 10));
  EXPECT_EQ(UNKNOWN_TYPE, beyond.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kNoGroup, beyond.group());

  // In-between value.  Missing from enum but within range.  Error condition.
  AutofillType between(static_cast<ServerFieldType>(16));
  EXPECT_EQ(UNKNOWN_TYPE, between.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kNoGroup, between.group());
}

TEST(AutofillTypeTest, HtmlFieldTypes) {
  // Unknown type.
  AutofillType unknown(HtmlFieldType::kUnspecified, HtmlFieldMode::kNone);
  EXPECT_EQ(UNKNOWN_TYPE, unknown.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kNoGroup, unknown.group());

  // Type with group but no subgroup.
  AutofillType first(HtmlFieldType::kGivenName, HtmlFieldMode::kNone);
  EXPECT_EQ(NAME_FIRST, first.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kName, first.group());

  // Type with group and subgroup.
  AutofillType phone(HtmlFieldType::kTel, HtmlFieldMode::kNone);
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER, phone.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kPhoneHome, phone.group());

  // Last value, to check any offset errors.
  AutofillType last(HtmlFieldType::kCreditCardExp4DigitYear,
                    HtmlFieldMode::kNone);
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR, last.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kCreditCard, last.group());

  // Shipping mode.
  AutofillType shipping_first(HtmlFieldType::kGivenName,
                              HtmlFieldMode::kShipping);
  EXPECT_EQ(NAME_FIRST, shipping_first.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kName, shipping_first.group());

  // Billing mode.
  AutofillType billing_first(HtmlFieldType::kGivenName,
                             HtmlFieldMode::kBilling);
  EXPECT_EQ(NAME_FIRST, billing_first.GetStorableType());
  EXPECT_EQ(FieldTypeGroup::kNameBilling, billing_first.group());
}

}  // namespace
}  // namespace autofill
