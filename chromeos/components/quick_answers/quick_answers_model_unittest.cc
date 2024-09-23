// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_model.h"

#include <string>

#include "base/values.h"
#include "chromeos/components/quick_answers/test/unit_conversion_unittest_constants.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeUnitName[] = "FakeUnit";
constexpr double kFakeUnitRateA = 1000;
constexpr double kFakeUnitRateB = 1;
constexpr double kFakeUnitRateC = 1;

constexpr char kPhoneticsInfoAudioUrl[] = "https://example.com/";
constexpr char kPhoneticsInfoQueryText[] = "QueryText";
constexpr char kPhoneticsInfoLocale[] = "Locale";

}  // namespace

namespace quick_answers {

TEST(UnitConversionTest,
     UnitConversionShouldReturnCorrectApproximateMultiplicationFormula) {
  std::optional<ConversionRule> source_rule =
      ConversionRule::Create(kMassCategory, kKilogramName, kKilogramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<ConversionRule> dest_rule =
      ConversionRule::Create(kMassCategory, kPoundName, kPoundRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<UnitConversion> unit_conversion =
      UnitConversion::Create(source_rule.value(), dest_rule.value());

  std::string expected_formula_text =
      "For an approximate result, multiply the mass value by 2.205";
  std::optional<std::string> conversion_formula_text =
      unit_conversion.value().GetConversionFormulaText();

  ASSERT_TRUE(conversion_formula_text);
  EXPECT_EQ(expected_formula_text, conversion_formula_text.value());
}

TEST(UnitConversionTest,
     UnitConversionShouldReturnCorrectExactMultiplicationFormula) {
  std::optional<ConversionRule> source_rule =
      ConversionRule::Create(kMassCategory, kKilogramName, kKilogramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<ConversionRule> dest_rule =
      ConversionRule::Create(kMassCategory, kGramName, kGramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<UnitConversion> unit_conversion =
      UnitConversion::Create(source_rule.value(), dest_rule.value());

  std::string expected_formula_text = "Multiply the mass value by 1000";
  std::optional<std::string> conversion_formula_text =
      unit_conversion.value().GetConversionFormulaText();

  ASSERT_TRUE(conversion_formula_text);
  EXPECT_EQ(expected_formula_text, conversion_formula_text.value());
}

TEST(UnitConversionTest,
     UnitConversionShouldReturnCorrectApproximateDivisionFormula) {
  std::optional<ConversionRule> source_rule =
      ConversionRule::Create(kMassCategory, kPoundName, kPoundRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<ConversionRule> dest_rule =
      ConversionRule::Create(kMassCategory, kKilogramName, kKilogramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<UnitConversion> unit_conversion =
      UnitConversion::Create(source_rule.value(), dest_rule.value());

  std::string expected_formula_text =
      "For an approximate result, divide the mass value by 2.205";
  std::optional<std::string> conversion_formula_text =
      unit_conversion.value().GetConversionFormulaText();

  ASSERT_TRUE(conversion_formula_text);
  EXPECT_EQ(expected_formula_text, conversion_formula_text.value());
}

TEST(UnitConversionTest,
     UnitConversionShouldReturnCorrectExactDivisionFormula) {
  std::optional<ConversionRule> source_rule =
      ConversionRule::Create(kMassCategory, kGramName, kGramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<ConversionRule> dest_rule =
      ConversionRule::Create(kMassCategory, kKilogramName, kKilogramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<UnitConversion> unit_conversion =
      UnitConversion::Create(source_rule.value(), dest_rule.value());

  std::string expected_formula_text = "Divide the mass value by 1000";
  std::optional<std::string> conversion_formula_text =
      unit_conversion.value().GetConversionFormulaText();

  ASSERT_TRUE(conversion_formula_text);
  EXPECT_EQ(expected_formula_text, conversion_formula_text.value());
}

TEST(UnitConversionTest,
     UnitConversionForLargeConversionsShouldReturnCorrectFormula) {
  std::optional<ConversionRule> source_rule =
      ConversionRule::Create(kMassCategory, kFakeUnitName, kFakeUnitRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<ConversionRule> dest_rule =
      ConversionRule::Create(kMassCategory, kGramName, kGramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<UnitConversion> unit_conversion =
      UnitConversion::Create(source_rule.value(), dest_rule.value());

  std::string expected_formula_text = "Multiply the mass value by 1e+06";
  std::optional<std::string> conversion_formula_text =
      unit_conversion.value().GetConversionFormulaText();

  ASSERT_TRUE(conversion_formula_text);
  EXPECT_EQ(expected_formula_text, conversion_formula_text.value());
}

TEST(UnitConversionTest, UnitConversionWithTermBShouldReturnNulloptForFormula) {
  std::optional<ConversionRule> source_rule =
      ConversionRule::Create(kMassCategory, kFakeUnitName, kFakeUnitRateA,
                             kFakeUnitRateB, /*term_c=*/std::nullopt);
  std::optional<ConversionRule> dest_rule =
      ConversionRule::Create(kMassCategory, kGramName, kGramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<UnitConversion> unit_conversion =
      UnitConversion::Create(source_rule.value(), dest_rule.value());

  std::optional<std::string> conversion_formula_text =
      unit_conversion.value().GetConversionFormulaText();
  EXPECT_FALSE(conversion_formula_text);
}

TEST(UnitConversionTest, UnitConversionWithTermCShouldReturnNulloptForFormula) {
  std::optional<ConversionRule> source_rule = ConversionRule::Create(
      kMassCategory, kFakeUnitName, /*term_a=*/std::nullopt,
      /*term_b=*/std::nullopt, kFakeUnitRateC);
  std::optional<ConversionRule> dest_rule =
      ConversionRule::Create(kMassCategory, kGramName, kGramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<UnitConversion> unit_conversion =
      UnitConversion::Create(source_rule.value(), dest_rule.value());

  std::optional<std::string> conversion_formula_text =
      unit_conversion.value().GetConversionFormulaText();
  EXPECT_FALSE(conversion_formula_text);
}

TEST(UnitConversionTest,
     UnitConversionBetweenIdenticalUnitsShouldReturnNulloptForFormula) {
  std::optional<ConversionRule> source_rule =
      ConversionRule::Create(kMassCategory, kGramName, kGramRateA,
                             /*term_b=*/std::nullopt, /*term_c=*/std::nullopt);
  std::optional<UnitConversion> unit_conversion =
      UnitConversion::Create(source_rule.value(), source_rule.value());

  std::optional<std::string> conversion_formula_text =
      unit_conversion.value().GetConversionFormulaText();
  EXPECT_FALSE(conversion_formula_text);
}

TEST(PhoneticsInfoTest, PhoneticsAudioUrl) {
  PhoneticsInfo phonetics_info;
  phonetics_info.phonetics_audio = GURL(kPhoneticsInfoAudioUrl);

  EXPECT_TRUE(phonetics_info.PhoneticsInfoAvailable());
  EXPECT_TRUE(phonetics_info.AudioUrlAvailable());
  EXPECT_FALSE(phonetics_info.TtsAudioAvailable());
}

TEST(PhoneticsInfoTest, HasTtsAudioButDisabled) {
  PhoneticsInfo phonetics_info;
  ASSERT_FALSE(phonetics_info.tts_audio_enabled)
      << "tts_audio_enabled is false by default";
  phonetics_info.locale = kPhoneticsInfoLocale;
  phonetics_info.query_text = kPhoneticsInfoQueryText;

  EXPECT_FALSE(phonetics_info.PhoneticsInfoAvailable());
  EXPECT_FALSE(phonetics_info.AudioUrlAvailable());
  EXPECT_FALSE(phonetics_info.TtsAudioAvailable());
}

TEST(PhoneticsInfoTest, TtsAudio) {
  PhoneticsInfo phonetics_info;
  phonetics_info.tts_audio_enabled = true;
  phonetics_info.locale = kPhoneticsInfoLocale;
  phonetics_info.query_text = kPhoneticsInfoQueryText;

  EXPECT_TRUE(phonetics_info.PhoneticsInfoAvailable());
  EXPECT_FALSE(phonetics_info.AudioUrlAvailable());
  EXPECT_TRUE(phonetics_info.TtsAudioAvailable());
}

TEST(PhoneticsInfoTest, Empty) {
  PhoneticsInfo phonetics_info;

  EXPECT_FALSE(phonetics_info.PhoneticsInfoAvailable());
  EXPECT_FALSE(phonetics_info.AudioUrlAvailable());
  EXPECT_FALSE(phonetics_info.TtsAudioAvailable());
}

}  // namespace quick_answers
