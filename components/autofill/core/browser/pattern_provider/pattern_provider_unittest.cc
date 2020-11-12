// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"
#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"
#include "components/autofill/core/browser/pattern_provider/test_pattern_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

MatchingPattern GetCompanyPatternEn() {
  autofill::MatchingPattern m_p;
  m_p.pattern_identifier = "kCompanyPatternEn";
  m_p.positive_pattern = "company|business|organization|organisation";
  m_p.positive_score = 1.1f;
  m_p.negative_pattern = "";
  m_p.match_field_attributes = MATCH_NAME;
  m_p.match_field_input_types = MATCH_TEXT;
  m_p.language = "en";
  return m_p;
}

MatchingPattern GetCompanyPatternDe() {
  autofill::MatchingPattern m_p;
  m_p.pattern_identifier = "kCompanyPatternDe";
  m_p.positive_pattern = "|(?<!con)firma|firmenname";
  m_p.positive_score = 1.1f;
  m_p.negative_pattern = "";
  m_p.match_field_attributes = MATCH_LABEL | MATCH_NAME;
  m_p.match_field_input_types = MATCH_TEXT;
  m_p.language = "de";
  return m_p;
}

// Pattern Provider with custom values set for testing.
class UnitTestPatternProvider : public PatternProvider {
 public:
  UnitTestPatternProvider();
  UnitTestPatternProvider(const std::vector<MatchingPattern>& de_patterns,
                          const std::vector<MatchingPattern>& en_patterns);
  ~UnitTestPatternProvider();
};

UnitTestPatternProvider::UnitTestPatternProvider()
    : UnitTestPatternProvider({GetCompanyPatternDe()},
                              {GetCompanyPatternEn()}) {}

UnitTestPatternProvider::UnitTestPatternProvider(
    const std::vector<MatchingPattern>& de_patterns,
    const std::vector<MatchingPattern>& en_patterns) {
  PatternProvider::SetPatternProviderForTesting(this);
  Map patterns;
  auto& company_patterns = patterns[AutofillType(COMPANY_NAME).ToString()];
  company_patterns["de"] = de_patterns;
  company_patterns["en"] = en_patterns;
  SetPatterns(patterns, base::Version(), true);
}

UnitTestPatternProvider::~UnitTestPatternProvider() {
  PatternProvider::ResetPatternProvider();
}

}  // namespace

class AutofillPatternProviderTest : public testing::Test {
 protected:
  UnitTestPatternProvider pattern_provider_;

  ~AutofillPatternProviderTest() override = default;
};

bool operator==(const MatchingPattern& mp1, const MatchingPattern& mp2) {
  return (mp1.language == mp2.language &&
          mp1.match_field_attributes == mp2.match_field_attributes &&
          mp1.match_field_input_types == mp2.match_field_input_types &&
          mp1.negative_pattern == mp2.negative_pattern &&
          mp1.pattern_identifier == mp2.pattern_identifier &&
          mp1.positive_pattern == mp2.positive_pattern &&
          mp1.positive_score == mp2.positive_score);
}

TEST(AutofillPatternProvider, Single_Match) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillUsePageLanguageToSelectFieldParsingPatterns);

  MatchingPattern kCompanyPatternEn = GetCompanyPatternEn();
  MatchingPattern kCompanyPatternDe = GetCompanyPatternDe();
  UnitTestPatternProvider* pattern_provider = new UnitTestPatternProvider();
  auto pattern_store = pattern_provider->GetMatchPatterns("COMPANY_NAME", "en");

  ASSERT_EQ(pattern_store.size(), 1u);
  EXPECT_EQ(pattern_store[0], kCompanyPatternEn);
}

// Test that the default pattern provider loads without crashing.
TEST(AutofillPatternProviderPipelineTest, DefaultPatternProviderLoads) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Enable so that PatternProvider::GetInstance() actually does load the JSON.
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillUsePageLanguageToSelectFieldParsingPatterns);

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  base::RunLoop run_loop;
  field_type_parsing::PopulateFromResourceBundle(run_loop.QuitClosure());
  run_loop.Run();
  PatternProvider& default_pattern_provider = PatternProvider::GetInstance();

  EXPECT_FALSE(default_pattern_provider.patterns().empty());

  // Call the getter to ensure sequence checks work correctly.
  default_pattern_provider.GetMatchPatterns("EMAIL_ADDRESS", "en");
}

// Test that the TestPatternProvider class uses a PatternProvider::Map
// equivalent to the DefaultPatternProvider. This is also an example of what is
// needed to test the DefaultPatternProvider. Warning: If this crashes, check
// that no state carried over from other tests using the singleton.
TEST(AutofillPatternProviderPipelineTest, TestParsingEquivalent) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Enable so that PatternProvider::GetInstance() actually does load the JSON.
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillUsePageLanguageToSelectFieldParsingPatterns);

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  base::RunLoop run_loop;
  field_type_parsing::PopulateFromResourceBundle(run_loop.QuitClosure());
  run_loop.Run();
  PatternProvider& default_pattern_provider = PatternProvider::GetInstance();

  TestPatternProvider test_pattern_provider;

  EXPECT_EQ(default_pattern_provider.patterns(),
            test_pattern_provider.patterns());
}

TEST(AutofillPatternProvider, BasedOnMatchType) {
  UnitTestPatternProvider p;
  ASSERT_GT(p.GetAllPatternsByType("COMPANY_NAME").size(), 0u);
  EXPECT_EQ(p.GetAllPatternsByType("COMPANY_NAME"),
            std::vector<MatchingPattern>(
                {GetCompanyPatternDe(), GetCompanyPatternEn()}));
  EXPECT_EQ(p.GetAllPatternsByType("COMPANY_NAME").size(), 2u);
}

TEST(AutofillPatternProvider, UnknownLanguages) {
  {
    base::test::ScopedFeatureList feature;
    feature.InitWithFeatures(
        // enabled
        {features::kAutofillUsePageLanguageToSelectFieldParsingPatterns},
        // disabled
        {features::
             kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics});
    UnitTestPatternProvider p;
    EXPECT_EQ(p.GetMatchPatterns("COMPANY_NAME", ""),
              p.GetAllPatternsByType("COMPANY_NAME"));
    EXPECT_EQ(p.GetMatchPatterns("COMPANY_NAME", "blabla"),
              p.GetAllPatternsByType("COMPANY_NAME"));
  }

  {
    base::test::ScopedFeatureList feature;
    feature.InitWithFeatures(
        // enabled
        {features::
             kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics},
        // disabled
        {features::kAutofillUsePageLanguageToSelectFieldParsingPatterns});
    UnitTestPatternProvider p;
    EXPECT_EQ(p.GetMatchPatterns("COMPANY_NAME", ""),
              p.GetAllPatternsByType("COMPANY_NAME"));
    EXPECT_EQ(p.GetMatchPatterns("COMPANY_NAME", "blabla"),
              p.GetAllPatternsByType("COMPANY_NAME"));
  }
}

TEST(AutofillPatternProvider, EnrichPatternsWithEnVersion) {
  {
    base::test::ScopedFeatureList feature;
    feature.InitWithFeatures(
        // enabled
        {features::kAutofillUsePageLanguageToSelectFieldParsingPatterns},
        // disabled
        {features::
             kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics});
    UnitTestPatternProvider p;
    EXPECT_EQ(p.GetMatchPatterns("COMPANY_NAME", "en"),
              std::vector<MatchingPattern>{GetCompanyPatternEn()});
    EXPECT_EQ(p.GetMatchPatterns("COMPANY_NAME", "de"),
              std::vector<MatchingPattern>(
                  {GetCompanyPatternDe(), GetCompanyPatternEn()}));
  }

  {
    base::test::ScopedFeatureList feature;
    feature.InitWithFeatures(
        // enabled
        {features::
             kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics},
        // disabled
        {features::kAutofillUsePageLanguageToSelectFieldParsingPatterns});
    UnitTestPatternProvider p;
    EXPECT_EQ(p.GetMatchPatterns("COMPANY_NAME", "en"),
              std::vector<MatchingPattern>({GetCompanyPatternDe(),
                                            GetCompanyPatternEn()}));
    EXPECT_EQ(p.GetMatchPatterns("COMPANY_NAME", "de"),
              std::vector<MatchingPattern>({GetCompanyPatternDe(),
                                            GetCompanyPatternEn()}));
  }
}

TEST(AutofillPatternProvider, SortPatternsByScore) {
  base::test::ScopedFeatureList feature;
  feature.InitWithFeatures(
      // enabled
      {features::kAutofillUsePageLanguageToSelectFieldParsingPatterns,
       features::kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics},
      // disabled
      {});
  std::vector<MatchingPattern> de_input_patterns;
  de_input_patterns.push_back(GetCompanyPatternDe());
  de_input_patterns.push_back(GetCompanyPatternDe());
  de_input_patterns.push_back(GetCompanyPatternDe());
  de_input_patterns.push_back(GetCompanyPatternDe());
  de_input_patterns[0].positive_score = 3.0;
  de_input_patterns[1].positive_score = 1.0;
  de_input_patterns[2].positive_score = 5.0;
  de_input_patterns[3].positive_score = 3.0;
  UnitTestPatternProvider p(de_input_patterns, {});
  const std::vector<MatchingPattern>& de_patterns =
      p.GetMatchPatterns(COMPANY_NAME, "de");
  ASSERT_EQ(de_patterns.size(), de_input_patterns.size());
  EXPECT_EQ(de_patterns[0].positive_score, 5.0);
  EXPECT_EQ(de_patterns[1].positive_score, 3.0);
  EXPECT_EQ(de_patterns[2].positive_score, 3.0);
  EXPECT_EQ(de_patterns[3].positive_score, 1.0);
}

}  // namespace autofill
