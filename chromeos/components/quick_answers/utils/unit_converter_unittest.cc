// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/unit_converter.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {
namespace {

using base::Value;
using Type = base::Value::Type;

constexpr char kMassCategory[] = "Mass";
constexpr char kLengthCategory[] = "Length";

constexpr char kFakeUnitName[] = "FakeUnit";
constexpr double kKilogramRateA = 1.0;
constexpr char kKilogramName[] = "Kilogram";
constexpr double kPoundRateA = 0.45359237;
constexpr char kPoundName[] = "Pound";
constexpr double kOunceRateA = 0.028349523125;
constexpr char kOunceName[] = "Ounce";

constexpr double kSamplePreferredRange = 100;
constexpr double kStrictPreferredRange = 1.001;
constexpr double kConvertSouceValue = 100;

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

}  // namespace

class UnitConverterTest : public testing::Test {
 public:
  UnitConverterTest() : rule_set_(Type::LIST) {}

  UnitConverterTest(const UnitConverterTest&) = delete;
  UnitConverterTest& operator=(const UnitConverterTest&) = delete;

  UnitConverter* CreateUnitConverter() {
    converter_ = std::make_unique<UnitConverter>(rule_set_);

    return converter_.get();
  }

  void AddConversion(const std::string& category, Value units) {
    Value conversion(Type::DICTIONARY);
    conversion.SetStringKey(kCategoryPath, category);
    conversion.SetKey(kUnitsPath, std::move(units));

    rule_set_.Append(std::move(conversion));
  }

 private:
  Value rule_set_;
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
  auto* category_name = conversion->FindStringPath(kCategoryPath);
  EXPECT_EQ(*category_name, kMassCategory);
}

TEST_F(UnitConverterTest, GetPossibleUnitsWithEmptyRulesetShouldReturnNullptr) {
  auto* converter = CreateUnitConverter();

  auto* units = converter->GetPossibleUnitsForCategory(kMassCategory);
  EXPECT_EQ(units, nullptr);
}

TEST_F(UnitConverterTest, GetPossibleUnitsWithKnownCategoryShouldSuccess) {
  Value input_units(Type::LIST);
  input_units.Append(CreateUnit(kKilogramRateA, kKilogramName));

  AddConversion(kMassCategory, std::move(input_units));
  auto* converter = CreateUnitConverter();

  auto* units = converter->GetPossibleUnitsForCategory(kMassCategory);
  EXPECT_EQ(units->GetList().size(), 1u);
  auto* unit = &units->GetList()[0];
  EXPECT_EQ(unit->FindDoublePath(kConversionRateAPath), kKilogramRateA);
  EXPECT_EQ(*unit->FindStringPath(kNamePath), kKilogramName);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitWithEmptyRulesetShouldReturnNullptr) {
  auto* converter = CreateUnitConverter();

  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramRateA, kKilogramName, kMassCategory),
      kSamplePreferredRange);
  EXPECT_EQ(unit, nullptr);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitForSameUnitShouldReturnNullptr) {
  Value input_units(Type::LIST);
  input_units.Append(CreateUnit(kKilogramRateA, kKilogramName));

  AddConversion(kMassCategory, std::move(input_units));
  auto* converter = CreateUnitConverter();

  // Should ignore the source unit itself in the ruleset.
  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramRateA, kKilogramName, kMassCategory),
      kSamplePreferredRange);
  EXPECT_EQ(unit, nullptr);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitWithProperUnitsShouldSuccess) {
  Value input_units(Type::LIST);
  input_units.Append(CreateUnit(kKilogramRateA, kKilogramName));
  input_units.Append(CreateUnit(kPoundRateA, kPoundName));

  AddConversion(kMassCategory, std::move(input_units));
  auto* converter = CreateUnitConverter();

  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramRateA, kKilogramName, kMassCategory),
      kSamplePreferredRange);
  EXPECT_EQ(unit->FindDoublePath(kConversionRateAPath), kPoundRateA);
  EXPECT_EQ(*unit->FindStringPath(kNamePath), kPoundName);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitForEmptySourceUnitShouldReturnNullptr) {
  Value input_units(Type::LIST);
  input_units.Append(CreateUnit(kKilogramRateA, kKilogramName));
  input_units.Append(CreateUnit(kPoundRateA, kPoundName));

  AddConversion(kMassCategory, std::move(input_units));
  auto* converter = CreateUnitConverter();

  // Should find nothing for empty source unit.
  auto* unit = converter->FindProperDestinationUnit(Value(Type::DICTIONARY),
                                                    kSamplePreferredRange);
  EXPECT_EQ(unit, nullptr);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitForStrictRangeShouldReturnNullptr) {
  Value input_units(Type::LIST);
  input_units.Append(CreateUnit(kKilogramRateA, kKilogramName));
  input_units.Append(CreateUnit(kPoundRateA, kPoundName));

  AddConversion(kMassCategory, std::move(input_units));
  auto* converter = CreateUnitConverter();

  // No unit within the preferred conversion rate found.
  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramRateA, kKilogramName, kMassCategory),
      kStrictPreferredRange);
  EXPECT_EQ(unit, nullptr);
}

TEST_F(UnitConverterTest,
       FindProperDestinationUnitBetweenMultipleUnitsShouldReturnClosestRate) {
  Value input_units(Type::LIST);
  input_units.Append(CreateUnit(kKilogramRateA, kKilogramName));
  input_units.Append(CreateUnit(kPoundRateA, kPoundName));
  input_units.Append(CreateUnit(kOunceRateA, kOunceName));

  AddConversion(kMassCategory, std::move(input_units));
  auto* converter = CreateUnitConverter();

  // Should return the unit with closest conversion rate, which is Pound.
  auto* unit = converter->FindProperDestinationUnit(
      CreateUnit(kKilogramRateA, kKilogramName, kMassCategory), 100);
  EXPECT_EQ(unit->FindDoublePath(kConversionRateAPath), kPoundRateA);
  EXPECT_EQ(*unit->FindStringPath(kNamePath), kPoundName);
}

TEST_F(UnitConverterTest, ConvertWithProperInputShouldSuccess) {
  auto* converter = CreateUnitConverter();

  auto result = converter->Convert(kConvertSouceValue,
                                   CreateUnit(kKilogramRateA, kKilogramName),
                                   CreateUnit(kPoundRateA, kPoundName));
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
      converter->Convert(kConvertSouceValue, CreateUnit(0, kFakeUnitName),
                         CreateUnit(kPoundRateA, kPoundName));
  EXPECT_EQ(result, std::string());
}

TEST_F(UnitConverterTest,
       ConvertWithZeroRateDestinationUnitShouldReturnEmptyResult) {
  auto* converter = CreateUnitConverter();

  // Should return empty result if input destination unit has 0 conversion rate.
  auto result = converter->Convert(kConvertSouceValue,
                                   CreateUnit(kPoundRateA, kPoundName),
                                   CreateUnit(0, kFakeUnitName));
  EXPECT_EQ(result, std::string());
}

}  // namespace quick_answers
