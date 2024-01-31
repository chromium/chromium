// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"

#define EXPECT_ROUNDED_DOUBLE_EQ(a, b)           \
  {                                              \
    double rounded_a = round(a * 100.0) / 100.0; \
    double rounded_b = round(b * 100.0) / 100.0; \
    EXPECT_DOUBLE_EQ(rounded_a, rounded_b);      \
  }

namespace quick_answers {
namespace {

using base::Value;
using Type = base::Value::Type;

constexpr char kMassCategory[] = "Mass";

constexpr double kKilogramRateA = 1.0;
constexpr char kKilogramName[] = "Kilogram";
constexpr double kPoundRateA = 0.45359237;
constexpr char kPoundName[] = "Pound";
constexpr double kGramRateA = 0.001;
constexpr char kGramName[] = "Gram";
constexpr double kOunceRateA = 0.028349523125;
constexpr char kOunceName[] = "Ounce";

constexpr double kSourceAmountKilogram = 100.0;
constexpr double kDestAmountPound = 220.462;
constexpr double kDestAmountGram = 100000;
constexpr double kDestAmountOunce = 3527.4;
constexpr char kSourceRawTextKilogram[] = "100 kilograms";
constexpr char kDestRawTextPound[] = "220.462 pounds";
constexpr char kDestRawTextGram[] = "100000 grams";

Value CreateUnit(double rate_a,
                 const std::string& name,
                 const std::string& category = std::string()) {
  Value::Dict unit;
  unit.Set(kConversionToSiAPath, rate_a);
  unit.Set(kNamePath, name);
  if (!category.empty())
    unit.Set(kCategoryPath, category);

  return Value(std::move(unit));
}

Value BuildMassRuleSet() {
  Value::List rule_set;
  Value::Dict conversion;
  Value::List units;

  conversion.Set(kCategoryPath, kMassCategory);
  units.Append(CreateUnit(kKilogramRateA, kKilogramName));
  units.Append(CreateUnit(kGramRateA, kGramName));
  units.Append(CreateUnit(kPoundRateA, kPoundName));
  units.Append(CreateUnit(kOunceRateA, kOunceName));
  conversion.Set(kUnitsPath, std::move(units));
  rule_set.Append(std::move(conversion));

  return Value(std::move(rule_set));
}

}  // namespace

class UnitConversionResultParserTest : public testing::Test {
 public:
  UnitConversionResultParserTest() = default;

  UnitConversionResultParserTest(const UnitConversionResultParserTest&) =
      delete;
  UnitConversionResultParserTest& operator=(
      const UnitConversionResultParserTest&) = delete;

  void SetCategory(const std::string& category) {
    result_.SetByDottedPath(kResultCategoryPath, category);
  }

  void SetSourceText(const std::string& text) {
    result_.SetByDottedPath(kSourceTextPath, text);
  }

  void SetDestText(const std::string& text) {
    result_.SetByDottedPath(kDestTextPath, text);
  }

  void SetSourceAmount(const double value) {
    result_.SetByDottedPath(kSourceAmountPath, value);
  }

  void SetDestAmount(const double value) {
    result_.SetByDottedPath(kDestAmountPath, value);
  }

  void AddSourceUnit(Value src_unit) {
    result_.SetByDottedPath(kSourceUnitPath, std::move(src_unit));
  }

  void AddDestUnit(Value dest_unit) {
    result_.SetByDottedPath(kDestUnitPath, std::move(dest_unit));
  }

  void AddRuleSet(Value rule_set) {
    result_.SetByDottedPath(kRuleSetPath, std::move(rule_set));
  }

  bool Parse(QuickAnswer* quick_answer) {
    return parser_.Parse(result_, quick_answer);
  }

  std::unique_ptr<StructuredResult> ParseInStructuredResult() {
    return parser_.ParseInStructuredResult(result_);
  }

  std::string GetAmountString(double amount) {
    return base::StringPrintf(kResultValueTemplate, amount);
  }

 protected:
  Value::Dict result_;

  UnitConversionResultParser parser_;
};

TEST_F(UnitConversionResultParserTest, ParseWithEmptyValueShouldReturnFalse) {
  QuickAnswer quick_answer;

  EXPECT_FALSE(Parse(&quick_answer));
}

TEST_F(UnitConversionResultParserTest,
       ParseWithIncorrectTypeShouldReturnFalse) {
  result_.SetByDottedPath(kDestTextPath, 1);
  QuickAnswer quick_answer;

  EXPECT_FALSE(Parse(&quick_answer));
}

TEST_F(UnitConversionResultParserTest,
       ParseWithIncorrectPathShouldReturnFalse) {
  result_.Set("WrongPath", kDestRawTextPound);
  QuickAnswer quick_answer;

  EXPECT_FALSE(Parse(&quick_answer));
}

TEST_F(UnitConversionResultParserTest,
       ParseWithNoSourceUnitShouldReturnRawText) {
  SetCategory(kMassCategory);
  SetDestText(kDestRawTextPound);
  SetSourceText(kSourceRawTextKilogram);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestAmount(kDestAmountPound);
  AddDestUnit(CreateUnit(kPoundRateA, kPoundName, kMassCategory));
  AddRuleSet(BuildMassRuleSet());

  QuickAnswer quick_answer;

  EXPECT_TRUE(Parse(&quick_answer));
  EXPECT_EQ(ResultType::kUnitConversionResult, quick_answer.result_type);

  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  EXPECT_EQ(0u, quick_answer.title.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(kDestRawTextPound,
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);

  // Expectations for `StructuredResult`.
  std::unique_ptr<StructuredResult> structured_result =
      ParseInStructuredResult();
  ASSERT_TRUE(structured_result);
  ASSERT_TRUE(structured_result->unit_conversion_result);

  UnitConversionResult* unit_conversion_result =
      structured_result->unit_conversion_result.get();
  EXPECT_EQ(unit_conversion_result->source_text, kSourceRawTextKilogram);
  EXPECT_EQ(unit_conversion_result->result_text,
            base::UTF16ToASCII(answer->text));
  EXPECT_EQ(unit_conversion_result->category, kMassCategory);
  EXPECT_EQ(unit_conversion_result->source_amount, kSourceAmountKilogram);
  EXPECT_FALSE(unit_conversion_result->source_to_dest_unit_conversion);
  EXPECT_TRUE(
      unit_conversion_result->alternative_unit_conversions_list.empty());
}

TEST_F(UnitConversionResultParserTest, ParseWithNoRuleSetShouldReturnRawText) {
  SetCategory(kMassCategory);
  SetDestText(kDestRawTextPound);
  SetSourceText(kSourceRawTextKilogram);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestAmount(kDestAmountPound);
  AddSourceUnit(CreateUnit(kKilogramRateA, kKilogramName, kMassCategory));
  AddDestUnit(CreateUnit(kPoundRateA, kPoundName, kMassCategory));

  QuickAnswer quick_answer;

  EXPECT_TRUE(Parse(&quick_answer));
  EXPECT_EQ(ResultType::kUnitConversionResult, quick_answer.result_type);

  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  EXPECT_EQ(0u, quick_answer.title.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(kDestRawTextPound,
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);

  // Expectations for `StructuredResult`.
  std::unique_ptr<StructuredResult> structured_result =
      ParseInStructuredResult();
  ASSERT_TRUE(structured_result);
  ASSERT_TRUE(structured_result->unit_conversion_result);

  UnitConversionResult* unit_conversion_result =
      structured_result->unit_conversion_result.get();
  EXPECT_EQ(unit_conversion_result->source_text, kSourceRawTextKilogram);
  EXPECT_EQ(unit_conversion_result->result_text,
            base::UTF16ToASCII(answer->text));
  EXPECT_EQ(unit_conversion_result->category, kMassCategory);
  EXPECT_EQ(unit_conversion_result->source_amount, kSourceAmountKilogram);

  ASSERT_TRUE(structured_result->unit_conversion_result
                  ->source_to_dest_unit_conversion);
  UnitConversion conversion_rate =
      unit_conversion_result->source_to_dest_unit_conversion.value();
  EXPECT_EQ(conversion_rate.category(), kMassCategory);
  EXPECT_EQ(conversion_rate.source_rule().unit_name(), kKilogramName);
  EXPECT_EQ(conversion_rate.dest_rule().unit_name(), kPoundName);
  EXPECT_ROUNDED_DOUBLE_EQ(
      conversion_rate.ConvertSourceAmountToDestAmount(kSourceAmountKilogram),
      kDestAmountPound);

  EXPECT_TRUE(
      unit_conversion_result->alternative_unit_conversions_list.empty());
}

TEST_F(UnitConversionResultParserTest,
       ParseWithResultWithinPreferredRangeShouldReturnRawText) {
  SetCategory(kMassCategory);
  SetDestText(kDestRawTextPound);
  SetSourceText(kSourceRawTextKilogram);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestAmount(kDestAmountPound);
  AddSourceUnit(CreateUnit(kKilogramRateA, kKilogramName, kMassCategory));
  AddDestUnit(CreateUnit(kPoundRateA, kPoundName, kMassCategory));
  AddRuleSet(BuildMassRuleSet());

  QuickAnswer quick_answer;

  EXPECT_TRUE(Parse(&quick_answer));
  EXPECT_EQ(ResultType::kUnitConversionResult, quick_answer.result_type);

  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  EXPECT_EQ(0u, quick_answer.title.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(kDestRawTextPound,
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);

  // Expectations for `StructuredResult`.
  std::unique_ptr<StructuredResult> structured_result =
      ParseInStructuredResult();
  ASSERT_TRUE(structured_result);
  ASSERT_TRUE(structured_result->unit_conversion_result);

  UnitConversionResult* unit_conversion_result =
      structured_result->unit_conversion_result.get();
  EXPECT_EQ(unit_conversion_result->source_text, kSourceRawTextKilogram);
  EXPECT_EQ(unit_conversion_result->result_text,
            base::UTF16ToASCII(answer->text));
  EXPECT_EQ(unit_conversion_result->category, kMassCategory);
  EXPECT_EQ(unit_conversion_result->source_amount, kSourceAmountKilogram);

  ASSERT_TRUE(structured_result->unit_conversion_result
                  ->source_to_dest_unit_conversion);
  UnitConversion conversion_rate =
      unit_conversion_result->source_to_dest_unit_conversion.value();
  EXPECT_EQ(conversion_rate.category(), kMassCategory);
  EXPECT_EQ(conversion_rate.source_rule().unit_name(), kKilogramName);
  EXPECT_EQ(conversion_rate.dest_rule().unit_name(), kPoundName);
  EXPECT_ROUNDED_DOUBLE_EQ(
      conversion_rate.ConvertSourceAmountToDestAmount(kSourceAmountKilogram),
      kDestAmountPound);

  EXPECT_FALSE(
      unit_conversion_result->alternative_unit_conversions_list.empty());
  std::vector<UnitConversion> alternative_conversions =
      unit_conversion_result->alternative_unit_conversions_list;
  EXPECT_EQ(2u, alternative_conversions.size());
  UnitConversion first_alt_conversion = alternative_conversions[0];
  EXPECT_EQ(first_alt_conversion.category(), kMassCategory);
  EXPECT_EQ(first_alt_conversion.source_rule().unit_name(), kKilogramName);
  EXPECT_EQ(first_alt_conversion.dest_rule().unit_name(), kOunceName);
  EXPECT_ROUNDED_DOUBLE_EQ(first_alt_conversion.ConvertSourceAmountToDestAmount(
                               kSourceAmountKilogram),
                           kDestAmountOunce);
  UnitConversion second_alt_conversion = alternative_conversions[1];
  EXPECT_EQ(second_alt_conversion.category(), kMassCategory);
  EXPECT_EQ(second_alt_conversion.source_rule().unit_name(), kKilogramName);
  EXPECT_EQ(second_alt_conversion.dest_rule().unit_name(), kGramName);
  EXPECT_ROUNDED_DOUBLE_EQ(
      second_alt_conversion.ConvertSourceAmountToDestAmount(
          kSourceAmountKilogram),
      kDestAmountGram);
}

TEST_F(UnitConversionResultParserTest,
       ParseWithResultOutOfPreferredRangeShouldReturnProperConversionResult) {
  SetCategory(kMassCategory);
  SetDestText(kDestRawTextGram);
  SetSourceText(kSourceRawTextKilogram);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestAmount(kDestAmountGram);
  AddSourceUnit(CreateUnit(kKilogramRateA, kKilogramName, kMassCategory));
  AddDestUnit(CreateUnit(kGramRateA, kGramName, kMassCategory));
  AddRuleSet(BuildMassRuleSet());

  QuickAnswer quick_answer;

  EXPECT_TRUE(Parse(&quick_answer));
  EXPECT_EQ(ResultType::kUnitConversionResult, quick_answer.result_type);

  auto expected_result = BuildUnitConversionResultText(
      base::StringPrintf(kResultValueTemplate, (kKilogramRateA / kPoundRateA) *
                                                   kSourceAmountKilogram),
      GetUnitDisplayText(kPoundName));

  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  EXPECT_EQ(0u, quick_answer.title.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(expected_result,
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);

  // Expectations for `StructuredResult`.
  std::unique_ptr<StructuredResult> structured_result =
      ParseInStructuredResult();
  ASSERT_TRUE(structured_result);
  ASSERT_TRUE(structured_result->unit_conversion_result);

  UnitConversionResult* unit_conversion_result =
      structured_result->unit_conversion_result.get();
  EXPECT_EQ(unit_conversion_result->source_text, kSourceRawTextKilogram);
  EXPECT_EQ(unit_conversion_result->result_text,
            base::UTF16ToASCII(answer->text));
  EXPECT_EQ(unit_conversion_result->category, kMassCategory);
  EXPECT_EQ(unit_conversion_result->source_amount, kSourceAmountKilogram);

  ASSERT_TRUE(structured_result->unit_conversion_result
                  ->source_to_dest_unit_conversion);
  UnitConversion conversion_rate =
      unit_conversion_result->source_to_dest_unit_conversion.value();
  EXPECT_EQ(conversion_rate.category(), kMassCategory);
  EXPECT_EQ(conversion_rate.source_rule().unit_name(), kKilogramName);
  EXPECT_EQ(conversion_rate.dest_rule().unit_name(), kPoundName);
  EXPECT_ROUNDED_DOUBLE_EQ(
      conversion_rate.ConvertSourceAmountToDestAmount(kSourceAmountKilogram),
      kDestAmountPound);

  EXPECT_FALSE(
      unit_conversion_result->alternative_unit_conversions_list.empty());
  std::vector<UnitConversion> alternative_conversions =
      unit_conversion_result->alternative_unit_conversions_list;
  EXPECT_EQ(2u, alternative_conversions.size());
  UnitConversion first_alt_conversion = alternative_conversions[0];
  EXPECT_EQ(first_alt_conversion.category(), kMassCategory);
  EXPECT_EQ(first_alt_conversion.source_rule().unit_name(), kKilogramName);
  EXPECT_EQ(first_alt_conversion.dest_rule().unit_name(), kOunceName);
  EXPECT_ROUNDED_DOUBLE_EQ(first_alt_conversion.ConvertSourceAmountToDestAmount(
                               kSourceAmountKilogram),
                           kDestAmountOunce);
  UnitConversion second_alt_conversion = alternative_conversions[1];
  EXPECT_EQ(second_alt_conversion.category(), kMassCategory);
  EXPECT_EQ(second_alt_conversion.source_rule().unit_name(), kKilogramName);
  EXPECT_EQ(second_alt_conversion.dest_rule().unit_name(), kGramName);
  EXPECT_ROUNDED_DOUBLE_EQ(
      second_alt_conversion.ConvertSourceAmountToDestAmount(
          kSourceAmountKilogram),
      kDestAmountGram);
}

}  // namespace quick_answers
