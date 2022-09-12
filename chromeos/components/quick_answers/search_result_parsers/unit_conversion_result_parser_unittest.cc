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

constexpr double kSourceAmountKilogram = 100.0;
constexpr double kDestAmountPound = 220.462;
constexpr double kDestAmountGram = 100000;
constexpr char kDestRawTextPound[] = "220.462 pounds";
constexpr char kDestRawTextGram[] = "100000 grams";

Value CreateUnit(double rate_a,
                 const std::string& name,
                 const std::string& category = std::string()) {
  Value unit(Type::DICTIONARY);
  unit.SetDoubleKey(kConversionRateAPath, rate_a);
  unit.SetStringKey(kNamePath, name);
  if (!category.empty())
    unit.SetStringKey(kCategoryPath, category);

  return unit;
}

Value BuildMassRuleSet() {
  Value rule_set(Type::LIST);
  Value conversion(Type::DICTIONARY);
  Value units(Type::LIST);

  conversion.SetStringKey(kCategoryPath, kMassCategory);
  units.Append(CreateUnit(kKilogramRateA, kKilogramName));
  units.Append(CreateUnit(kGramRateA, kGramName));
  units.Append(CreateUnit(kPoundRateA, kPoundName));
  conversion.SetKey(kUnitsPath, std::move(units));
  rule_set.Append(std::move(conversion));

  return rule_set;
}

}  // namespace

class UnitConversionResultParserTest : public testing::Test {
 public:
  UnitConversionResultParserTest() : result_(Type::DICTIONARY) {}

  UnitConversionResultParserTest(const UnitConversionResultParserTest&) =
      delete;
  UnitConversionResultParserTest& operator=(
      const UnitConversionResultParserTest&) = delete;

  void SetDestText(const std::string& text) {
    result_.SetStringPath(kDestTextPath, text);
  }

  void SetSourceAmount(const double value) {
    result_.SetDoublePath(kSourceAmountPath, value);
  }

  void SetDestAmount(const double value) {
    result_.SetDoublePath(kDestAmountPath, value);
  }

  void AddSourceUnit(Value src_unit) {
    result_.SetPath(kSourceUnitPath, std::move(src_unit));
  }

  void AddRuleSet(Value rule_set) {
    result_.SetPath(kRuleSetPath, std::move(rule_set));
  }

  bool Parse(QuickAnswer* quick_answer) {
    return parser_.Parse(&result_, quick_answer);
  }

 protected:
  Value result_;

  UnitConversionResultParser parser_;
};

TEST_F(UnitConversionResultParserTest, ParseWithEmptyValueShouldReturnFalse) {
  QuickAnswer quick_answer;

  EXPECT_FALSE(Parse(&quick_answer));
}

TEST_F(UnitConversionResultParserTest,
       ParseWithIncorrectTypeShouldReturnFalse) {
  result_.SetIntPath(kDestTextPath, 1);
  QuickAnswer quick_answer;

  EXPECT_FALSE(Parse(&quick_answer));
}

TEST_F(UnitConversionResultParserTest,
       ParseWithIncorrectPathShouldReturnFalse) {
  result_.SetStringPath("WrongPath", kDestRawTextPound);
  QuickAnswer quick_answer;

  EXPECT_FALSE(Parse(&quick_answer));
}

TEST_F(UnitConversionResultParserTest,
       ParseWithNoSourceUnitShouldReturnRawText) {
  SetDestText(kDestRawTextPound);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestAmount(kDestAmountPound);
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
}

TEST_F(UnitConversionResultParserTest, ParseWithNoRuleSetShouldReturnRawText) {
  SetDestText(kDestRawTextPound);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestAmount(kDestAmountPound);
  AddSourceUnit(CreateUnit(kKilogramRateA, kKilogramName, kMassCategory));

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
}

TEST_F(UnitConversionResultParserTest,
       ParseWithResultWithinPreferredRangeShouldReturnRawText) {
  SetDestText(kDestRawTextPound);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestAmount(kDestAmountPound);
  AddSourceUnit(CreateUnit(kKilogramRateA, kKilogramName, kMassCategory));
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
}

TEST_F(UnitConversionResultParserTest,
       ParseWithResultOutOfPreferredRangeShouldReturnProperConversionResult) {
  SetDestText(kDestRawTextGram);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestAmount(kDestAmountGram);
  AddSourceUnit(CreateUnit(kKilogramRateA, kKilogramName, kMassCategory));
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
}

}  // namespace quick_answers
