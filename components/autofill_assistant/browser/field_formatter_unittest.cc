// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/field_formatter.h"

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace field_formatter {
namespace {
const char kFakeUrl[] = "https://www.example.com";

using ::testing::_;
using ::testing::Eq;
using ::testing::IsSupersetOf;

TEST(FieldFormatterTest, FormatString) {
  std::map<std::string, std::string> mappings = {{"keyA", "valueA"},
                                                 {"keyB", "valueB"},
                                                 {"keyC", "valueC"},
                                                 {"keyD", "$30.5"},
                                                 {"keyE", "30.5$"}
                                                 /* keyF does not exist */};

  EXPECT_EQ(*FormatString("", mappings), "");
  EXPECT_EQ(*FormatString("input", mappings), "input");
  EXPECT_EQ(*FormatString("prefix ${keyA}", mappings), "prefix valueA");
  EXPECT_EQ(*FormatString("prefix ${keyA}${keyB}${keyC} suffix", mappings),
            "prefix valueAvalueBvalueC suffix");
  EXPECT_EQ(*FormatString("keyA = ${keyA}", mappings), "keyA = valueA");
  EXPECT_EQ(*FormatString("Price: $${keyA}", mappings), "Price: $valueA");
  EXPECT_EQ(*FormatString("Price: ${keyD}", mappings), "Price: $30.5");
  EXPECT_EQ(*FormatString("Price: ${keyE}", mappings), "Price: 30.5$");
  EXPECT_EQ(FormatString("${keyF}", mappings), absl::nullopt);
  EXPECT_EQ(FormatString("${keyA}${keyF}", mappings), absl::nullopt);

  EXPECT_EQ(*FormatString("${keyF}", mappings, /*strict = */ false), "${keyF}");
  EXPECT_EQ(*FormatString("${keyA}${keyF}", mappings, /*strict = */ false),
            "valueA${keyF}");
  EXPECT_EQ(*FormatString("${keyF}${keyA}", mappings, /*strict = */ false),
            "${keyF}valueA");
}

TEST(FieldFormatterTest, FormatExpression) {
  std::map<std::string, std::string> mappings = {{"1", "valueA"},
                                                 {"2", "val.ueB"}};
  std::string result;

  ValueExpression value_expression_1;
  value_expression_1.add_chunk()->set_text("text");
  value_expression_1.add_chunk()->set_text(" ");
  value_expression_1.add_chunk()->set_key(1);
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_1, mappings,
                                             /* quote_meta=*/false, &result)
                                .proto_status());
  EXPECT_EQ("text valueA", result);

  ValueExpression value_expression_2;
  value_expression_2.add_chunk()->set_text("^");
  value_expression_2.add_chunk()->set_key(2);
  value_expression_2.add_chunk()->set_text("$");
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_2, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ("^val.ueB$", result);
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_2, mappings,
                                             /* quote_meta= */ true, &result)
                                .proto_status());
  EXPECT_EQ("^val\\.ueB$", result);

  ValueExpression value_expression_3;
  value_expression_3.add_chunk()->set_key(3);
  EXPECT_EQ(AUTOFILL_INFO_NOT_AVAILABLE,
            FormatExpression(value_expression_3, mappings,
                             /* quote_meta= */ false, &result)
                .proto_status());

  ValueExpression value_expression_4;
  EXPECT_EQ(ACTION_APPLIED, FormatExpression(value_expression_4, mappings,
                                             /* quote_meta= */ false, &result)
                                .proto_status());
  EXPECT_EQ(std::string(), result);
}

TEST(FieldFormatterTest, GetHumanReadableValueExpression) {
  ValueExpression value_expression;
  value_expression.add_chunk()->set_text("+");
  value_expression.add_chunk()->set_key(1);
  EXPECT_EQ(GetHumanReadableValueExpression(value_expression), "+${1}");
}

TEST(FieldFormatterTest, AutofillProfile) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "John", "", "Doe", "editor@gmail.com", "", "203 Barfield Lane",
      "", "Mountain View", "CA", "94043", "US", "+12345678901");

  // NAME_FIRST NAME_LAST
  EXPECT_EQ(
      *FormatString("${3} ${5}", CreateAutofillMappings(profile, "en-US")),
      "John Doe");

  // PHONE_HOME_COUNTRY_CODE, PHONE_HOME_CITY_CODE, PHONE_HOME_NUMBER
  EXPECT_EQ(*FormatString("(+${12}) (${11}) ${10}",
                          CreateAutofillMappings(profile, "en-US")),
            "(+1) (234) 5678901");

  // State handling from abbreviation.
  EXPECT_EQ(*FormatString("${34}", CreateAutofillMappings(profile, "en-US")),
            "CA");
  EXPECT_EQ(*FormatString("${-6}", CreateAutofillMappings(profile, "en-US")),
            "California");

  // State handling from state name.
  autofill::AutofillProfile state_name_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&state_name_profile, "John", "", "Doe", "", "",
                                 "", "", "", "California", "", "US", "");
  EXPECT_EQ(*FormatString("${34}",
                          CreateAutofillMappings(state_name_profile, "en-US")),
            "CA");
  EXPECT_EQ(*FormatString("${-6}",
                          CreateAutofillMappings(state_name_profile, "en-US")),
            "California");

  // Unknown state.
  autofill::AutofillProfile unknown_state_profile(base::GenerateGUID(),
                                                  kFakeUrl);
  autofill::test::SetProfileInfo(&unknown_state_profile, "John", "", "Doe", "",
                                 "", "", "", "", "XY", "", "US", "");
  EXPECT_EQ(FormatString("${34}", CreateAutofillMappings(unknown_state_profile,
                                                         "en-US")),
            absl::nullopt);
  EXPECT_EQ(FormatString("${-6}", CreateAutofillMappings(unknown_state_profile,
                                                         "en-US")),
            "XY");

  // UNKNOWN_TYPE
  EXPECT_EQ(FormatString("${1}", CreateAutofillMappings(profile, "en-US")),
            absl::nullopt);
}

TEST(FieldFormatterTest, CreditCard) {
  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "John Doe",
                                    "4111 1111 1111 1111", "01", "2050", "");

  // CREDIT_CARD_NAME_FULL
  EXPECT_EQ(
      *FormatString("${51}", CreateAutofillMappings(credit_card, "en-US")),
      "John Doe");

  // CREDIT_CARD_NUMBER
  EXPECT_EQ(
      *FormatString("${52}", CreateAutofillMappings(credit_card, "en-US")),
      "4111111111111111");

  // CREDIT_CARD_NUMBER_LAST_FOUR_DIGITS
  EXPECT_EQ(
      *FormatString("**** ${-4}", CreateAutofillMappings(credit_card, "en-US")),
      "**** 1111");

  // CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR
  EXPECT_EQ(*FormatString("${53}/${54}",
                          CreateAutofillMappings(credit_card, "en-US")),
            "01/50");

  // CREDIT_CARD_NETWORK, CREDIT_CARD_NETWORK_FOR_DISPLAY
  EXPECT_EQ(*FormatString("${-2} ${-5}",
                          CreateAutofillMappings(credit_card, "en-US")),
            "visa Visa");

  // CREDIT_CARD_NON_PADDED_EXP_MONTH
  EXPECT_EQ(
      *FormatString("${-7}", CreateAutofillMappings(credit_card, "en-US")),
      "1");
}

TEST(FieldFormatterTest, SpecialCases) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "John", "", "Doe", "editor@gmail.com", "", "203 Barfield Lane",
      "", "Mountain View", "CA", "94043", "US", "+12345678901");

  EXPECT_EQ(*FormatString("", CreateAutofillMappings(profile, "en-US")),
            std::string());
  EXPECT_EQ(*FormatString("${3}", CreateAutofillMappings(profile, "en-US")),
            "John");
  EXPECT_EQ(FormatString("${-1}", CreateAutofillMappings(profile, "en-US")),
            absl::nullopt);
  EXPECT_EQ(
      FormatString(
          "${" + base::NumberToString(autofill::MAX_VALID_FIELD_TYPE) + "}",
          CreateAutofillMappings(profile, "en-US")),
      absl::nullopt);

  // Second {} is not prefixed with $.
  EXPECT_EQ(
      *FormatString("${3} {10}", CreateAutofillMappings(profile, "en-US")),
      "John {10}");
}

TEST(FieldFormatterTest, DifferentLocales) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "John", "", "Doe", "editor@gmail.com", "", "203 Barfield Lane",
      "", "Mountain View", "CA", "94043", "US", "+12345678901");
  auto mappings = CreateAutofillMappings(profile, "de-DE");

  // 36 == ADDRESS_HOME_COUNTRY
  EXPECT_EQ(*FormatString("${36}", CreateAutofillMappings(profile, "en-US")),
            "United States");
  EXPECT_EQ(*FormatString("${36}", CreateAutofillMappings(profile, "de-DE")),
            "Vereinigte Staaten");

  // Invalid locales default to "en-US".
  EXPECT_EQ(*FormatString("${36}", CreateAutofillMappings(profile, "")),
            "United States");
  EXPECT_EQ(*FormatString("${36}", CreateAutofillMappings(profile, "invalid")),
            "United States");
}

TEST(FieldFormatterTest, AddsAllProfileFieldsUsAddress) {
  std::map<std::string, std::string> expected_values = {
      {"-6", "California"},
      {"3", "Alpha"},
      {"4", "Beta"},
      {"5", "Gamma"},
      {"6", "B"},
      {"7", "Alpha Beta Gamma"},
      {"9", "alpha@google.com"},
      {"10", "1234567890"},
      {"12", "1"},
      {"13", "1234567890"},
      {"14", "+11234567890"},
      {"30", "Amphitheatre Parkway 1600"},
      {"31", "Google Building 1"},
      {"33", "Mountain View"},
      {"34", "CA"},
      {"35", "94043"},
      {"36", "United States"},
      {"60", "Google"},
      {"77", "Amphitheatre Parkway 1600\nGoogle Building 1"}};

  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "Alpha", "Beta", "Gamma", "alpha@google.com", "Google",
      "Amphitheatre Parkway 1600", "Google Building 1", "Mountain View", "CA",
      "94043", "US", "+1 123-456-7890");

  EXPECT_THAT(CreateAutofillMappings(profile, "en-US"),
              IsSupersetOf(expected_values));
}

TEST(FieldFormatterTest, AddsAllProfileFieldsForNonUsAddress) {
  std::map<std::string, std::string> expected_values = {
      {"-6", "Canton Zurich"},
      {"3", "Alpha"},
      {"4", "Beta"},
      {"5", "Gamma"},
      {"6", "B"},
      {"7", "Alpha Beta Gamma"},
      {"9", "alpha@google.com"},
      {"10", "1234567"},
      {"11", "79"},
      {"12", "41"},
      {"13", "0791234567"},
      {"14", "+41791234567"},
      {"30", "Brandschenkestrasse 110"},
      {"31", "Google Building 110"},
      {"33", "Zurich"},
      {"35", "8002"},
      {"36", "Switzerland"},
      {"60", "Google"},
      {"77", "Brandschenkestrasse 110\nGoogle Building 110"}};

  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "Alpha", "Beta", "Gamma", "alpha@google.com", "Google",
      "Brandschenkestrasse 110", "Google Building 110", "Zurich",
      "Canton Zurich", "8002", "CH", "+41791234567");

  EXPECT_THAT(CreateAutofillMappings(profile, "en-US"),
              IsSupersetOf(expected_values));
}

TEST(FieldFormatterTest, AddsAllCreditCardFields) {
  std::map<std::string, std::string> expected_values = {
      {"-7", "8"},
      {"-5", "Visa"},
      {"-4", "1111"},
      {"-2", "visa"},
      {"51", "Alpha Beta Gamma"},
      {"52", "4111111111111111"},
      {"53", "08"},
      {"54", "50"},
      {"55", "2050"},
      {"56", "08/50"},
      {"57", "08/2050"},
      {"58", "Visa"},
      {"91", "Alpha"},
      {"92", "Gamma"}};

  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Alpha Beta Gamma",
                                    "4111111111111111", "8", "2050", "");

  EXPECT_THAT(CreateAutofillMappings(credit_card, "en-US"),
              IsSupersetOf(expected_values));
}

}  // namespace
}  // namespace field_formatter
}  // namespace autofill_assistant
