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
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"
#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"
#include "components/autofill/core/browser/pattern_provider/test_pattern_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Pattern Provider with custom values set for testing.
class UnitTestPatternProvider : public PatternProvider {
 public:
  UnitTestPatternProvider();
  ~UnitTestPatternProvider();
};

UnitTestPatternProvider::UnitTestPatternProvider() {
  auto& company_patterns = patterns_[AutofillType(COMPANY_NAME).ToString()];
  company_patterns["EN"].push_back(GetCompanyPatternEn());
  company_patterns["DE"].push_back(GetCompanyPatternDe());

  PatternProvider::SetPatternProviderForTesting(this);
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
  auto pattern_store = pattern_provider->GetMatchPatterns("COMPANY_NAME", "EN");

  ASSERT_EQ(pattern_store.size(), 1u);
  EXPECT_EQ(pattern_store[0], kCompanyPatternEn);
}

// Test that the default pattern provider loads without crashing.
TEST(AutofillPatternProviderPipelineTest, DefaultPatternProviderLoads) {
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  base::RunLoop run_loop;
  field_type_parsing::PopulateFromResourceBundle(run_loop.QuitClosure());
  run_loop.Run();
  PatternProvider& default_pattern_provider = PatternProvider::GetInstance();

  EXPECT_FALSE(default_pattern_provider.patterns_.empty());

  // Call the getter to ensure sequence checks work correctly.
  default_pattern_provider.GetMatchPatterns("EMAIL_ADDRESS", "en");
}

// Test that the TestPatternProvider class uses a PatternProvider::Map
// equivalent to the DefaultPatternProvider. This is also an example of what is
// needed to test the DefaultPatternProvider. Warning: If this crashes, check
// that no state carried over from other tests using the singleton.
TEST(AutofillPatternProviderPipelineTest, TestParsingEquivalent) {
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  base::RunLoop run_loop;
  field_type_parsing::PopulateFromResourceBundle(run_loop.QuitClosure());
  run_loop.Run();
  PatternProvider& default_pattern_provider = PatternProvider::GetInstance();

  TestPatternProvider test_pattern_provider;

  EXPECT_EQ(default_pattern_provider.patterns_,
            test_pattern_provider.patterns_);
}

TEST(AutofillPatternProvider, Based_On_Type_Match) {
  MatchingPattern kCompanyPatternEn = GetCompanyPatternEn();
  MatchingPattern kCompanyPatternDe = GetCompanyPatternDe();
  std::vector<MatchingPattern> match_vector;
  match_vector.push_back(kCompanyPatternDe);
  match_vector.push_back(kCompanyPatternEn);

  UnitTestPatternProvider* pattern_provider = new UnitTestPatternProvider();

  ASSERT_GT(pattern_provider->GetAllPatternsBaseOnType("COMPANY_NAME").size(),
            0u);
  EXPECT_EQ(pattern_provider->GetAllPatternsBaseOnType("COMPANY_NAME"),
            match_vector);
  EXPECT_EQ(pattern_provider->GetAllPatternsBaseOnType("COMPANY_NAME").size(),
            2u);
}

}  // namespace autofill
