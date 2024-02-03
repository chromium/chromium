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
#include "chromeos/components/quick_answers/test/unit_conversion_unittest_constants.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
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

inline constexpr char kTemperatureCategory[] = "Temperature";

inline constexpr double kCelciusRateA = 1.0;
inline constexpr double kCelciusRateB = 273.15;
inline constexpr char kCelciusName[] = "Degree Celcius";
inline constexpr double kFahrenheitRateA = 0.5555555555555556;
inline constexpr double kFahrenheitRateB = 255.3722222222222;
inline constexpr char kFahrenheitName[] = "Fahrenheit";
inline constexpr double kKelvinRateA = 1.0;
inline constexpr char kKelvinName[] = "Kelvin";

inline constexpr double kSourceAmountCelcius = 10.0;
inline constexpr double kDestAmountFahrenheit = 50.0;
inline constexpr double kDestAmountKelvin = 283.15;
inline constexpr char kSourceRawTextCelcius[] = "10 degrees celcius";
inline constexpr char kDestRawTextFahrenheit[] = "50 degrees fahrenheit";

inline constexpr char kFuelEconomyCategory[] = "Fuel Economy";

inline constexpr double kLiterPer100KilometersRateC = 100.0;
inline constexpr char kLiterPer100KilometersName[] = "Liter per 100 kilometers";
inline constexpr double kKilometerPerLiterRateA = 1.0;
inline constexpr char kKilometerPerLiterName[] = "Kilometer per liter";

inline constexpr double kSourceAmountLiterPer100Kilometers = 1.0;
inline constexpr double kDestAmountKilometerPerLiter = 100.0;
inline constexpr char kSourceRawTextLiterPer100Kilometers[] =
    "1 liter per 100 kilometers";
inline constexpr char kDestRawTextKilometerPerLiter[] =
    "100 kilometers per liter";

Value BuildMassRuleSet() {
  Value::List rule_set;
  Value::Dict conversion;
  Value::List units;

  conversion.Set(kCategoryPath, kMassCategory);
  units.Append(CreateUnit(kKilogramName, kKilogramRateA));
  units.Append(CreateUnit(kGramName, kGramRateA));
  units.Append(CreateUnit(kPoundName, kPoundRateA));
  units.Append(CreateUnit(kOunceName, kOunceRateA));
  conversion.Set(kUnitsPath, std::move(units));
  rule_set.Append(std::move(conversion));

  return Value(std::move(rule_set));
}

Value BuildTemperatureRuleSet() {
  Value::List rule_set;
  Value::Dict conversion;
  Value::List units;

  conversion.Set(kCategoryPath, kTemperatureCategory);
  units.Append(CreateUnit(kCelciusName, kCelciusRateA, kCelciusRateB));
  units.Append(CreateUnit(kFahrenheitName, kFahrenheitRateA, kFahrenheitRateB));
  units.Append(CreateUnit(kKelvinName, kKelvinRateA));
  conversion.Set(kUnitsPath, std::move(units));
  rule_set.Append(std::move(conversion));

  return Value(std::move(rule_set));
}

Value BuildFuelEconomyRuleSet() {
  Value::List rule_set;
  Value::Dict conversion;
  Value::List units;

  conversion.Set(kCategoryPath, kFuelEconomyCategory);
  units.Append(
      CreateUnit(kLiterPer100KilometersName, /*rate_a=*/kInvalidRateTermValue,
                 /*rate_b=*/kInvalidRateTermValue, /*category=*/std::string(),
                 kLiterPer100KilometersRateC));
  units.Append(CreateUnit(kKilometerPerLiterName, kKilometerPerLiterRateA));
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

  void AddSourceUnit(Value::Dict src_unit) {
    result_.SetByDottedPath(kSourceUnitPath, std::move(src_unit));
  }

  void AddDestUnit(Value::Dict dest_unit) {
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
  SetSourceText(kSourceRawTextKilogram);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestText(kDestRawTextPound);
  SetDestAmount(kDestAmountPound);
  AddDestUnit(CreateUnit(kPoundName, kPoundRateA,
                         /*rate_b=*/kInvalidRateTermValue, kMassCategory));
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
  SetSourceText(kSourceRawTextKilogram);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestText(kDestRawTextPound);
  SetDestAmount(kDestAmountPound);
  AddSourceUnit(CreateUnit(kKilogramName, kKilogramRateA,
                           /*rate_b=*/kInvalidRateTermValue, kMassCategory));
  AddDestUnit(CreateUnit(kPoundName, kPoundRateA,
                         /*rate_b=*/kInvalidRateTermValue, kMassCategory));

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
  SetSourceText(kSourceRawTextKilogram);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestText(kDestRawTextPound);
  SetDestAmount(kDestAmountPound);
  AddSourceUnit(CreateUnit(kKilogramName, kKilogramRateA,
                           /*rate_b=*/kInvalidRateTermValue, kMassCategory));
  AddDestUnit(CreateUnit(kPoundName, kPoundRateA,
                         /*rate_b=*/kInvalidRateTermValue, kMassCategory));
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
  SetSourceText(kSourceRawTextKilogram);
  SetSourceAmount(kSourceAmountKilogram);
  SetDestText(kDestRawTextGram);
  SetDestAmount(kDestAmountGram);
  AddSourceUnit(CreateUnit(kKilogramName, kKilogramRateA,
                           /*rate_b=*/kInvalidRateTermValue, kMassCategory));
  AddDestUnit(CreateUnit(kGramName, kGramRateA,
                         /*rate_b=*/kInvalidRateTermValue, kMassCategory));
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

TEST_F(UnitConversionResultParserTest,
       ParseWithMultiVariableConversionsShouldReturnProperConversionResult) {
  SetCategory(kTemperatureCategory);
  SetSourceText(kSourceRawTextCelcius);
  SetSourceAmount(kSourceAmountCelcius);
  SetDestText(kDestRawTextFahrenheit);
  SetDestAmount(kDestAmountFahrenheit);
  AddSourceUnit(CreateUnit(kCelciusName, kCelciusRateA, kCelciusRateB,
                           kTemperatureCategory));
  AddDestUnit(CreateUnit(kFahrenheitName, kFahrenheitRateA, kFahrenheitRateB,
                         kTemperatureCategory));
  AddRuleSet(BuildTemperatureRuleSet());

  QuickAnswer quick_answer;

  EXPECT_TRUE(Parse(&quick_answer));
  EXPECT_EQ(ResultType::kUnitConversionResult, quick_answer.result_type);

  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  EXPECT_EQ(0u, quick_answer.title.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(kDestRawTextFahrenheit,
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);

  // Expectations for `StructuredResult`.
  std::unique_ptr<StructuredResult> structured_result =
      ParseInStructuredResult();
  ASSERT_TRUE(structured_result);
  ASSERT_TRUE(structured_result->unit_conversion_result);

  UnitConversionResult* unit_conversion_result =
      structured_result->unit_conversion_result.get();
  EXPECT_EQ(unit_conversion_result->source_text, kSourceRawTextCelcius);
  EXPECT_EQ(unit_conversion_result->result_text,
            base::UTF16ToASCII(answer->text));
  EXPECT_EQ(unit_conversion_result->category, kTemperatureCategory);
  EXPECT_EQ(unit_conversion_result->source_amount, kSourceAmountCelcius);

  ASSERT_TRUE(structured_result->unit_conversion_result
                  ->source_to_dest_unit_conversion);
  UnitConversion conversion_rate =
      unit_conversion_result->source_to_dest_unit_conversion.value();
  EXPECT_EQ(conversion_rate.category(), kTemperatureCategory);
  EXPECT_EQ(conversion_rate.source_rule().unit_name(), kCelciusName);
  EXPECT_EQ(conversion_rate.dest_rule().unit_name(), kFahrenheitName);
  EXPECT_ROUNDED_DOUBLE_EQ(
      conversion_rate.ConvertSourceAmountToDestAmount(kSourceAmountCelcius),
      kDestAmountFahrenheit);

  EXPECT_FALSE(
      unit_conversion_result->alternative_unit_conversions_list.empty());
  std::vector<UnitConversion> alternative_conversions =
      unit_conversion_result->alternative_unit_conversions_list;
  EXPECT_EQ(1u, alternative_conversions.size());
  UnitConversion alt_conversion = alternative_conversions[0];
  EXPECT_EQ(alt_conversion.category(), kTemperatureCategory);
  EXPECT_EQ(alt_conversion.source_rule().unit_name(), kCelciusName);
  EXPECT_EQ(alt_conversion.dest_rule().unit_name(), kKelvinName);
  EXPECT_ROUNDED_DOUBLE_EQ(
      alt_conversion.ConvertSourceAmountToDestAmount(kSourceAmountCelcius),
      kDestAmountKelvin);
}

TEST_F(UnitConversionResultParserTest,
       ParseWithNonLinearConversionsShouldReturnProperConversionResult) {
  SetCategory(kFuelEconomyCategory);
  SetSourceText(kSourceRawTextLiterPer100Kilometers);
  SetSourceAmount(kSourceAmountLiterPer100Kilometers);
  SetDestText(kDestRawTextKilometerPerLiter);
  SetDestAmount(kDestAmountKilometerPerLiter);
  AddSourceUnit(CreateUnit(kLiterPer100KilometersName,
                           /*rate_a=*/kInvalidRateTermValue,
                           /*rate_b=*/kInvalidRateTermValue,
                           kFuelEconomyCategory, kLiterPer100KilometersRateC));
  AddDestUnit(CreateUnit(kKilometerPerLiterName, kKilometerPerLiterRateA,
                         /*rate_b=*/kInvalidRateTermValue,
                         kFuelEconomyCategory));
  AddRuleSet(BuildFuelEconomyRuleSet());

  QuickAnswer quick_answer;

  EXPECT_TRUE(Parse(&quick_answer));
  EXPECT_EQ(ResultType::kUnitConversionResult, quick_answer.result_type);

  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  EXPECT_EQ(0u, quick_answer.title.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ(kDestRawTextKilometerPerLiter,
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(ui::kColorLabelForegroundSecondary, answer->color_id);

  // Expectations for `StructuredResult`.
  std::unique_ptr<StructuredResult> structured_result =
      ParseInStructuredResult();
  ASSERT_TRUE(structured_result);
  ASSERT_TRUE(structured_result->unit_conversion_result);

  UnitConversionResult* unit_conversion_result =
      structured_result->unit_conversion_result.get();
  EXPECT_EQ(unit_conversion_result->source_text,
            kSourceRawTextLiterPer100Kilometers);
  EXPECT_EQ(unit_conversion_result->result_text,
            base::UTF16ToASCII(answer->text));
  EXPECT_EQ(unit_conversion_result->category, kFuelEconomyCategory);
  EXPECT_EQ(unit_conversion_result->source_amount,
            kSourceAmountLiterPer100Kilometers);

  ASSERT_TRUE(structured_result->unit_conversion_result
                  ->source_to_dest_unit_conversion);
  UnitConversion conversion_rate =
      unit_conversion_result->source_to_dest_unit_conversion.value();
  EXPECT_EQ(conversion_rate.category(), kFuelEconomyCategory);
  EXPECT_EQ(conversion_rate.source_rule().unit_name(),
            kLiterPer100KilometersName);
  EXPECT_EQ(conversion_rate.dest_rule().unit_name(), kKilometerPerLiterName);
  EXPECT_ROUNDED_DOUBLE_EQ(conversion_rate.ConvertSourceAmountToDestAmount(
                               kSourceAmountLiterPer100Kilometers),
                           kDestAmountKilometerPerLiter);

  EXPECT_TRUE(
      unit_conversion_result->alternative_unit_conversions_list.empty());
}

}  // namespace quick_answers
