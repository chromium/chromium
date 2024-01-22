// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/birthdate_field_parser.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/form_parsing/parsing_test_utils.h"

namespace autofill {

namespace {

// Returns a vector of consecutive SelectOption items with values and content in
// [`low`, `high`] in either in- or decreasing order.
std::vector<SelectOption> GetSelectOptionRange(int low,
                                               int high,
                                               bool increasing) {
  std::vector<SelectOption> options;
  for (int i = 0; i <= high - low; i++) {
    std::u16string value =
        base::NumberToString16(increasing ? low + i : high - i);
    options.push_back({value, value});
  }
  return options;
}

std::vector<SelectOption> GetDays() {
  return GetSelectOptionRange(1, 31, /*increasing=*/true);
}
std::vector<SelectOption> GetMonths() {
  return GetSelectOptionRange(1, 12, /*increasing=*/true);
}
std::vector<SelectOption> GetYears() {
  return GetSelectOptionRange(1900, 2022, /*increasing=*/false);
}

}  // namespace

class BirthdateFieldParserTest
    : public FormFieldParserTestBase,
      public testing::TestWithParam<PatternProviderFeatureState> {
 public:
  BirthdateFieldParserTest() : FormFieldParserTestBase(GetParam()) {}
  BirthdateFieldParserTest(const BirthdateFieldParserTest&) = delete;
  BirthdateFieldParserTest& operator=(const BirthdateFieldParserTest&) = delete;

 protected:
  std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                         AutofillScanner* scanner) override {
    return BirthdateFieldParser::Parse(context, scanner);
  }
};

INSTANTIATE_TEST_SUITE_P(
    BirthdateFieldParserTest,
    BirthdateFieldParserTest,
    ::testing::ValuesIn(PatternProviderFeatureState::All()));

TEST_P(BirthdateFieldParserTest, ParseDMY) {
  AddSelectOneFormFieldData("", "", GetDays(), BIRTHDATE_DAY);
  AddSelectOneFormFieldData("", "", GetMonths(), BIRTHDATE_MONTH);
  AddSelectOneFormFieldData("", "", GetYears(), BIRTHDATE_4_DIGIT_YEAR);
  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(BirthdateFieldParserTest, ParseYMD) {
  AddSelectOneFormFieldData("", "", GetYears(), BIRTHDATE_4_DIGIT_YEAR);
  AddSelectOneFormFieldData("", "", GetMonths(), BIRTHDATE_MONTH);
  AddSelectOneFormFieldData("", "", GetDays(), BIRTHDATE_DAY);
  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(BirthdateFieldParserTest, DefaultOptions) {
  auto days = GetDays();
  days.insert(days.begin(), {u"", u"Day"});
  auto months = GetMonths();
  months.insert(months.begin(), {u"", u"Month"});
  auto years = GetYears();
  years.insert(years.begin(), {u"", u"Year"});
  AddSelectOneFormFieldData("", "", days, BIRTHDATE_DAY);
  AddSelectOneFormFieldData("", "", months, BIRTHDATE_MONTH);
  AddSelectOneFormFieldData("", "", years, BIRTHDATE_4_DIGIT_YEAR);
  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(BirthdateFieldParserTest, LeadingZeros) {
  // Replace the first 9 day entries with 01-09.
  auto days = GetDays();
  ASSERT_GE(days.size(), 9u);
  for (int i = 0; i < 9; i++) {
    std::u16string value = u"0" + base::NumberToString16(i + 1);
    days[i] = {value, value};
  }
  AddSelectOneFormFieldData("", "", days, BIRTHDATE_DAY);
  AddSelectOneFormFieldData("", "", GetMonths(), BIRTHDATE_MONTH);
  AddSelectOneFormFieldData("", "", GetYears(), BIRTHDATE_4_DIGIT_YEAR);
  ClassifyAndVerify(ParseResult::PARSED);
}

TEST_P(BirthdateFieldParserTest, TooManyOptions) {
  auto days = GetDays();
  days.insert(days.begin(), {u"", u"Hello"});
  days.insert(days.begin(), {u"", u"World"});
  EXPECT_GT(days.size(), 13u);  // Too many options for a day selector.
  AddSelectOneFormFieldData("", "", days, BIRTHDATE_DAY);
  AddSelectOneFormFieldData("", "", GetMonths(), BIRTHDATE_MONTH);
  AddSelectOneFormFieldData("", "", GetYears(), BIRTHDATE_4_DIGIT_YEAR);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(BirthdateFieldParserTest, MissingYear) {
  AddSelectOneFormFieldData("", "", GetDays(), BIRTHDATE_DAY);
  AddSelectOneFormFieldData("", "", GetMonths(), BIRTHDATE_MONTH);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

TEST_P(BirthdateFieldParserTest, IncompleteMonth) {
  auto months = GetMonths();
  months.resize(5);
  AddSelectOneFormFieldData("", "", GetDays(), BIRTHDATE_DAY);
  AddSelectOneFormFieldData("", "", months, BIRTHDATE_MONTH);
  AddSelectOneFormFieldData("", "", GetYears(), BIRTHDATE_4_DIGIT_YEAR);
  ClassifyAndVerify(ParseResult::NOT_PARSED);
}

}  // namespace autofill
