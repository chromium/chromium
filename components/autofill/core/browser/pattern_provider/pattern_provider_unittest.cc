// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/task/thread_pool.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"
#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/language_code.h"
#include "components/grit/components_resources.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

namespace {

LanguageCode kLanguageDe("de");
LanguageCode kLanguageEn("en");

MatchingPattern GetCompanyPatternEn() {
  autofill::MatchingPattern m_p;
  m_p.positive_pattern = u"company|business|organization|organisation";
  m_p.positive_score = 1.1;
  m_p.negative_pattern = u"";
  m_p.match_field_attributes = MATCH_NAME;
  m_p.match_field_input_types = MATCH_TEXT;
  m_p.language = kLanguageEn;
  return m_p;
}

MatchingPattern GetCompanyPatternDe() {
  autofill::MatchingPattern m_p;
  m_p.positive_pattern = u"|(?<!con)firma|firmenname";
  m_p.positive_score = 1.1;
  m_p.negative_pattern = u"";
  m_p.match_field_attributes = MATCH_LABEL | MATCH_NAME;
  m_p.match_field_input_types = MATCH_TEXT;
  m_p.language = kLanguageDe;
  return m_p;
}

// Pattern Provider with custom values set for testing.
class UnitTestPatternProvider : public PatternProvider {
 public:
  UnitTestPatternProvider()
      : UnitTestPatternProvider({GetCompanyPatternDe()},
                                {GetCompanyPatternEn()}) {}

  UnitTestPatternProvider(const std::vector<MatchingPattern>& de_patterns,
                          const std::vector<MatchingPattern>& en_patterns) {
    Map patterns;
    auto& company_patterns =
        patterns[AutofillType::ServerFieldTypeToString(COMPANY_NAME)];
    company_patterns[kLanguageDe] = de_patterns;
    company_patterns[kLanguageEn] = en_patterns;
    SetPatterns(std::move(patterns), base::Version());
  }
};

// Called when the JSON bundle has been parsed, and sets the PatternProvider's
// patterns.
void OnJsonParsed(base::OnceClosure done_callback,
                  data_decoder::DataDecoder::ValueOrError result) {
  base::Version version =
      field_type_parsing::ExtractVersionFromJsonObject(result.value.value());
  absl::optional<PatternProvider::Map> patterns =
      field_type_parsing::GetConfigurationFromJsonObject(result.value.value());
  ASSERT_TRUE(patterns);
  ASSERT_TRUE(version.IsValid());
  PatternProvider& pattern_provider = PatternProvider::GetInstance();
  pattern_provider.SetPatterns(std::move(patterns.value()), std::move(version));
  std::move(done_callback).Run();
}

// Loads the string from the Resource Bundle on a worker thread.
void LoadPatternsFromResourceBundle() {
  ASSERT_TRUE(ui::ResourceBundle::HasSharedInstance());
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ui::ResourceBundle::LoadDataResourceString,
                     base::Unretained(&bundle), IDR_AUTOFILL_REGEX_JSON),
      base::BindOnce(
          [](base::OnceClosure done_callback, std::string resource_string) {
            data_decoder::DataDecoder::ParseJsonIsolated(
                std::move(resource_string),
                base::BindOnce(&OnJsonParsed, std::move(done_callback)));
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

bool operator==(const MatchingPattern& mp1, const MatchingPattern& mp2) {
  return mp1.language == mp2.language &&
         mp1.positive_pattern == mp2.positive_pattern &&
         mp1.negative_pattern == mp2.negative_pattern &&
         mp1.positive_score == mp2.positive_score &&
         mp1.match_field_attributes == mp2.match_field_attributes &&
         mp1.match_field_input_types == mp2.match_field_input_types;
}

TEST(AutofillPatternProviderTest, Single_Match) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillParsingPatternsLanguageDependent);

  UnitTestPatternProvider p;
  EXPECT_THAT(p.GetMatchPatterns("COMPANY_NAME", kLanguageEn),
              ::testing::ElementsAre(GetCompanyPatternEn()));
  EXPECT_THAT(
      p.GetMatchPatterns("COMPANY_NAME", kLanguageDe),
      ::testing::ElementsAre(GetCompanyPatternDe(), GetCompanyPatternEn()));
}

TEST(AutofillPatternProviderTest, BasedOnMatchType) {
  UnitTestPatternProvider p;
  EXPECT_THAT(
      p.GetAllPatternsByType("COMPANY_NAME"),
      ::testing::ElementsAre(GetCompanyPatternDe(), GetCompanyPatternEn()));
}

TEST(AutofillPatternProviderTest, TestDefaultEqualsJson) {
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  auto default_version = PatternProvider::GetInstance().pattern_version_;
  auto default_patterns = PatternProvider::GetInstance().patterns_;

  // We want to make sure that the JSON loading actually does set the patterns.
  // To this end, manipulate the current patterns. Then |default_patterns| can
  // only be identical to |json_patterns| if loading the JSON updates the
  // patterns.
  PatternProvider::GetInstance().patterns_.clear();
  ASSERT_NE(default_patterns, PatternProvider::GetInstance().patterns_);

  // Load the JSON explicitly from the file.
  LoadPatternsFromResourceBundle();

  auto json_version = PatternProvider::GetInstance().pattern_version_;
  auto json_patterns = PatternProvider::GetInstance().patterns_;

  EXPECT_FALSE(default_version.IsValid());
  EXPECT_TRUE(json_version.IsValid());
  EXPECT_EQ(default_patterns, json_patterns);
}

TEST(AutofillPatternProviderTest, UnknownLanguages) {
  {
    base::test::ScopedFeatureList feature;
    feature.InitWithFeatures(
        // enabled
        {features::kAutofillParsingPatternsLanguageDependent},
        // disabled
        {features::kAutofillParsingPatternsNegativeMatching});
    UnitTestPatternProvider p;
    EXPECT_EQ(p.GetMatchPatterns(COMPANY_NAME, LanguageCode("")),
              p.GetAllPatternsByType(COMPANY_NAME));
    EXPECT_EQ(p.GetMatchPatterns(COMPANY_NAME, LanguageCode("io")),
              p.GetAllPatternsByType(COMPANY_NAME));
  }

  {
    base::test::ScopedFeatureList feature;
    feature.InitWithFeatures(
        // enabled
        {features::kAutofillParsingPatternsNegativeMatching},
        // disabled
        {features::kAutofillParsingPatternsLanguageDependent});
    UnitTestPatternProvider p;
    EXPECT_EQ(p.GetMatchPatterns(COMPANY_NAME, LanguageCode("")),
              p.GetAllPatternsByType(COMPANY_NAME));
    EXPECT_EQ(p.GetMatchPatterns(COMPANY_NAME, LanguageCode("io")),
              p.GetAllPatternsByType(COMPANY_NAME));
  }
}

TEST(AutofillPatternProviderTest, EnrichPatternsWithEnVersion) {
  {
    base::test::ScopedFeatureList feature;
    feature.InitWithFeatures(
        // enabled
        {features::kAutofillParsingPatternsLanguageDependent},
        // disabled
        {features::kAutofillParsingPatternsNegativeMatching});
    UnitTestPatternProvider p;
    EXPECT_EQ(p.GetMatchPatterns(COMPANY_NAME, kLanguageEn),
              std::vector<MatchingPattern>{GetCompanyPatternEn()});
    EXPECT_EQ(p.GetMatchPatterns(COMPANY_NAME, kLanguageDe),
              std::vector<MatchingPattern>(
                  {GetCompanyPatternDe(), GetCompanyPatternEn()}));
  }

  {
    base::test::ScopedFeatureList feature;
    feature.InitWithFeatures(
        // enabled
        {features::kAutofillParsingPatternsNegativeMatching},
        // disabled
        {features::kAutofillParsingPatternsLanguageDependent});
    UnitTestPatternProvider p;
    EXPECT_EQ(p.GetMatchPatterns(COMPANY_NAME, kLanguageEn),
              std::vector<MatchingPattern>(
                  {GetCompanyPatternDe(), GetCompanyPatternEn()}));
    EXPECT_EQ(p.GetMatchPatterns(COMPANY_NAME, kLanguageDe),
              std::vector<MatchingPattern>(
                  {GetCompanyPatternDe(), GetCompanyPatternEn()}));
  }
}

TEST(AutofillPatternProviderTest, SortPatternsByScore) {
  base::test::ScopedFeatureList feature;
  feature.InitWithFeatures(
      // enabled
      {features::kAutofillParsingPatternsLanguageDependent,
       features::kAutofillParsingPatternsNegativeMatching},
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
      p.GetMatchPatterns(COMPANY_NAME, kLanguageDe);
  ASSERT_EQ(de_patterns.size(), de_input_patterns.size());
  EXPECT_EQ(de_patterns[0].positive_score, 5.0);
  EXPECT_EQ(de_patterns[1].positive_score, 3.0);
  EXPECT_EQ(de_patterns[2].positive_score, 3.0);
  EXPECT_EQ(de_patterns[3].positive_score, 1.0);
}

}  // namespace autofill
