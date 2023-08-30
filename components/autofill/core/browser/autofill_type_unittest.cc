// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_type.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Matcher;
using ::testing::Property;
using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

// TODO(crbug.com/1466435): Consolidate the prediction matchers used in
// different files and move them to a central location.
Matcher<FieldPrediction> EqualsPrediction(ServerFieldType prediction) {
  return AllOf(Property("type", &FieldPrediction::type, prediction),
               Property("source", &FieldPrediction::source,
                        FieldPrediction::SOURCE_AUTOFILL_DEFAULT));
}

class AutofillTypeServerPredictionTest : public ::testing::Test {
 private:
  test::AutofillUnitTestEnvironment autofill_environment_;
};

TEST_F(AutofillTypeServerPredictionTest, PredictionFromAutofillField) {
  AutofillField field = AutofillField(
      test::CreateTestFormField("label", "name", "value", /*type=*/"text"));
  field.set_server_predictions(
      {test::CreateFieldPrediction(ServerFieldType::EMAIL_ADDRESS),
       test::CreateFieldPrediction(ServerFieldType::USERNAME)});
  field.set_may_use_prefilled_placeholder(true);

  AutofillType::ServerPrediction prediction(field);
  EXPECT_THAT(prediction.server_predictions,
              ElementsAre(EqualsPrediction(ServerFieldType::EMAIL_ADDRESS),
                          EqualsPrediction(ServerFieldType::USERNAME)));
  EXPECT_TRUE(prediction.may_use_prefilled_placeholder);
}

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
  EXPECT_EQ(FieldTypeGroup::kPhone, phone.group());

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
  EXPECT_EQ(FieldTypeGroup::kPhone, phone.group());

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
  EXPECT_EQ(FieldTypeGroup::kName, billing_first.group());
}

class AutofillTypeTestForHtmlFieldTypes
    : public ::testing::TestWithParam<
          std::tuple<std::underlying_type_t<HtmlFieldType>,
                     std::underlying_type_t<HtmlFieldMode>>> {
 public:
  HtmlFieldType html_field_type() const {
    return static_cast<HtmlFieldType>(std::get<0>(GetParam()));
  }

  HtmlFieldMode html_field_mode() const {
    return static_cast<HtmlFieldMode>(std::get<1>(GetParam()));
  }
};

INSTANTIATE_TEST_SUITE_P(
    AutofillTypeTest,
    AutofillTypeTestForHtmlFieldTypes,
    testing::Combine(
        testing::Range(base::to_underlying(HtmlFieldType::kMinValue),
                       base::to_underlying(HtmlFieldType::kMaxValue)),
        testing::Range(base::to_underlying(HtmlFieldMode::kMinValue),
                       base::to_underlying(HtmlFieldMode::kMaxValue))));

TEST_P(AutofillTypeTestForHtmlFieldTypes, GroupsOfHtmlFieldTypes) {
  // Some HtmlFieldTypes have no ServerFieldType representation.
  if (html_field_type() == HtmlFieldType::kFullAddress ||
      html_field_type() == HtmlFieldType::kOneTimeCode ||
      html_field_type() == HtmlFieldType::kTransactionAmount ||
      html_field_type() == HtmlFieldType::kTransactionCurrency) {
    return;
  }
  AutofillType t(html_field_type(), html_field_mode());
  SCOPED_TRACE(testing::Message()
               << "html_field_type="
               << FieldTypeToStringPiece(html_field_type()) << " "
               << "html_field_mode="
               << HtmlFieldModeToStringPiece(html_field_mode()) << " "
               << "field_type=" << FieldTypeToStringPiece(t.GetStorableType()));
  EXPECT_EQ(t.group(), GroupTypeOfServerFieldType(t.GetStorableType()));
}

}  // namespace
}  // namespace autofill
