// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/unit_converter.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/components/quick_answers/test/unit_conversion_unittest_constants.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {
namespace {

using base::Value;

constexpr char kLengthCategory[] = "Length";
constexpr char kFakeUnitName[] = "FakeUnit";

constexpr double kSamplePreferredRange = 100;
constexpr double kStrictPreferredRange = 1.001;
constexpr double kConvertSouceValue = 100;

}  // namespace

class UnitConverterTest : public testing::Test {
 public:
  UnitConverterTest() = default;

  UnitConverterTest(const UnitConverterTest&) = delete;
  UnitConverterTest& operator=(const UnitConverterTest&) = delete;

  UnitConverter* CreateUnitConverter() {
    converter_ = std::make_unique<UnitConverter>(rule_set_);

    return converter_.get();
  }

  void AddConversion(const std::string& category, Value units) {
    Value::Dict conversion;
    conversion.Set(kCategoryPath, category);
    conversion.Set(kUnitsPath, std::move(units));

    rule_set_.Append(std::move(conversion));
  }

 private:
  Value::List rule_set_;
  std::unique_ptr<UnitConverter> converter_;
};

TEST_F(UnitConverterTest, GetConversionWithEmptyRulesetShouldReturnNullptr) {
  auto* converter = CreateUnitConverter();
  auto* conversion = converter->GetConversionForCategory(kMassCategory);
  EXPECT_EQ(conversion, nullptr);
}

TEST_F(UnitConverterTest, GetConversionWithUnkownCategoryShouldReturnNullptr) {
  AddConversion(kMassCategory, Value());
  auto* converter = CreateUnitConverter();

  auto* conversion = converter->GetConversionForCategory(kLengthCategory);
  EXPECT_EQ(conversion, nullptr);
}

TEST_F(UnitConverterTest, GetConversionWithKnownCategoryShouldSucceed) {
  AddConversion(kMassCategory, Value());
  auto* converter = CreateUnitConverter();

  auto* conversion = converter->GetConversionForCategory(kMassCategory);
  auto* category_name = conversion->FindStringByDottedPath(kCategoryPath);
  EXPECT_EQ(*category_name, kMassCategory);
}

TEST_F(UnitConverterTest, GetPossibleUnitsWithEmptyRulesetShouldReturnNullptr) {
  auto* converter = CreateUnitConverter();

  auto* units = converter->GetPossibleUnitsForCategory(kMassCategory);
  EXPECT_EQ(units, nullptr);
}

TEST_F(UnitConverterTest, GetPossibleUnitsWithKnownCategoryShouldSuccess) {
  Value::List input_units;
  input_units.Append(CreateUnit(kKilogramName, kKilogramRateA));

  AddConversion(kMassCategory, Value(std::move(input_units)));
  auto* converter = CreateUnitConverter();

  auto* units = converter->GetPossibleUnitsForCategory(kMassCategory);
  EXPECT_EQ(units->size(), 1u);
  auto& unit = (*units)[0].GetDict();
  EXPECT_EQ(unit.FindDoubleByDottedPath(kConversionToSiAPath), kKilogramRateA);
  EXPECT_EQ(*unit.FindStringByDottedPath(kNamePath), kKilogramName);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitWithEmptyRulesetShouldReturnNullptr) {
  auto* converter = CreateUnitConverter();

  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramName, kKilogramRateA,
                 /*rate_b=*/kInvalidRateTermValue, kMassCategory),
      kSamplePreferredRange);
  EXPECT_EQ(unit, nullptr);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitForSameUnitShouldReturnNullptr) {
  Value::List input_units;
  input_units.Append(CreateUnit(kKilogramName, kKilogramRateA));

  AddConversion(kMassCategory, Value(std::move(input_units)));
  auto* converter = CreateUnitConverter();

  // Should ignore the source unit itself in the ruleset.
  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramName, kKilogramRateA,
                 /*rate_b=*/kInvalidRateTermValue, kMassCategory),
      kSamplePreferredRange);
  EXPECT_EQ(unit, nullptr);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitWithProperUnitsShouldSuccess) {
  Value::List input_units;
  input_units.Append(CreateUnit(kKilogramName, kKilogramRateA));
  input_units.Append(CreateUnit(kPoundName, kPoundRateA));

  AddConversion(kMassCategory, Value(std::move(input_units)));
  auto* converter = CreateUnitConverter();

  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramName, kKilogramRateA,
                 /*rate_b=*/kInvalidRateTermValue, kMassCategory),
      kSamplePreferredRange);
  EXPECT_EQ(unit->FindDoubleByDottedPath(kConversionToSiAPath), kPoundRateA);
  EXPECT_EQ(*unit->FindStringByDottedPath(kNamePath), kPoundName);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitForEmptySourceUnitShouldReturnNullptr) {
  Value::List input_units;
  input_units.Append(CreateUnit(kKilogramName, kKilogramRateA));
  input_units.Append(CreateUnit(kPoundName, kPoundRateA));

  AddConversion(kMassCategory, Value(std::move(input_units)));
  auto* converter = CreateUnitConverter();

  // Should find nothing for empty source unit.
  auto* unit = converter->FindProperDestinationUnit(Value::Dict(),
                                                    kSamplePreferredRange);
  EXPECT_EQ(unit, nullptr);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitForStrictRangeShouldReturnNullptr) {
  Value::List input_units;
  input_units.Append(CreateUnit(kKilogramName, kKilogramRateA));
  input_units.Append(CreateUnit(kPoundName, kPoundRateA));

  AddConversion(kMassCategory, Value(std::move(input_units)));
  auto* converter = CreateUnitConverter();

  // No unit within the preferred conversion rate found.
  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramName, kKilogramRateA,
                 /*rate_b=*/kInvalidRateTermValue, kMassCategory),
      kStrictPreferredRange);
  EXPECT_EQ(unit, nullptr);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitBetweenMultipleUnitsShouldReturnClosestRate) {
  Value::List input_units;
  input_units.Append(CreateUnit(kKilogramName, kKilogramRateA));
  input_units.Append(CreateUnit(kPoundName, kPoundRateA));
  input_units.Append(CreateUnit(kOunceName, kOunceRateA));

  AddConversion(kMassCategory, Value(std::move(input_units)));
  auto* converter = CreateUnitConverter();

  // Should return the unit with closest conversion rate, which is Pound.
  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramName, kKilogramRateA,
                 /*rate_b=*/kInvalidRateTermValue, kMassCategory),
      100);
  EXPECT_EQ(unit->FindDoubleByDottedPath(kConversionToSiAPath), kPoundRateA);
  EXPECT_EQ(*unit->FindStringByDottedPath(kNamePath), kPoundName);
}

TEST_F(UnitConverterTest, ConvertWithProperInputShouldSuccess) {
  auto* converter = CreateUnitConverter();

  auto result = converter->Convert(kConvertSouceValue,
                                   CreateUnit(kKilogramName, kKilogramRateA),
                                   CreateUnit(kPoundName, kPoundRateA));
  auto expected_result = BuildUnitConversionResultText(
      base::StringPrintf(kResultValueTemplate,
                         (kKilogramRateA / kPoundRateA) * kConvertSouceValue),
      GetUnitDisplayText(kPoundName));
  EXPECT_EQ(result, expected_result);
}

TEST_F(UnitConverterTest,
       ConvertWithZeroRateSourceUnitShouldReturnEmptyResult) {
  auto* converter = CreateUnitConverter();

  // Should return empty result if input source unit has 0 conversion rate.
  auto result =
      converter->Convert(kConvertSouceValue, CreateUnit(kFakeUnitName),
                         CreateUnit(kPoundName, kPoundRateA));
  EXPECT_EQ(result, std::string());
}

TEST_F(UnitConverterTest,
       ConvertWithZeroRateDestinationUnitShouldReturnEmptyResult) {
  auto* converter = CreateUnitConverter();

  // Should return empty result if input destination unit has 0 conversion rate.
  auto result = converter->Convert(kConvertSouceValue,
                                   CreateUnit(kPoundName, kPoundRateA),
                                   CreateUnit(kFakeUnitName));
  EXPECT_EQ(result, std::string());
}

}  // namespace quick_answers
