// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/pattern_provider/default_regex_patterns.h"

// Keep these tests in sync with
// components/autofill/core/browser/autofill_regexes_unittest.cc.
// Only these tests will be kept once the pattern provider launches.

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/ranges/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

struct PatternTestCase {
  // Reference to the pattern name in the resources/regex_patterns.json file.
  const char* pattern_name;
  // Language selector for the pattern, refers to the detected language of a
  // website.
  const char* language = "en";
  // Strings that should be matched by the pattern.
  std::vector<std::string> positive_samples = {};
  std::vector<std::string> negative_samples = {};
};

// Returns whether at least one of |patterns| matches |sample|.
bool MatchesAnyPattern(
    const std::string& sample,
    const std::vector<MatchingPattern>& patterns) {
  std::u16string utf16_sample = base::UTF8ToUTF16(sample);

  // Returns whether |pattern| matches |utf16_sample|.
  auto matches_sample = [&utf16_sample](const MatchingPattern& pattern) {
    // In case a negative pattern matches, the positive pattern is ignored.
    bool negative_match =
        !pattern.negative_pattern.empty() &&
        MatchesPattern(utf16_sample, pattern.negative_pattern);
    if (negative_match)
      return false;
    bool positive_match =
        !pattern.positive_pattern.empty() &&
        MatchesPattern(utf16_sample, pattern.positive_pattern);
    return positive_match;
  };

  return base::ranges::any_of(patterns, matches_sample);
}

}  // namespace

class DefaultRegExPatternsTest
    : public testing::TestWithParam<PatternTestCase> {
 public:
  void Validate(const std::string& pattern_name,
                const std::string& language,
                bool is_positive_sample,
                const std::string& sample);
};

void DefaultRegExPatternsTest::Validate(const std::string& pattern_name,
                                        const std::string& language,
                                        bool is_positive_sample,
                                        const std::string& sample) {
  SCOPED_TRACE(testing::Message()
               << pattern_name << "/" << language << ", "
               << (is_positive_sample ? "positive" : "negative")
               << " sample: " << sample);

  const std::vector<MatchingPattern> patterns =
      PatternProvider::GetInstance().GetMatchPatterns(pattern_name,
                                                       LanguageCode(language));

  EXPECT_EQ(is_positive_sample, MatchesAnyPattern(sample, patterns));
}

TEST_P(DefaultRegExPatternsTest, TestPositiveAndNegativeCases) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillParsingPatternsLanguageDependent,
                            features::kAutofillParsingPatternsNegativeMatching},
      /*disabled_features=*/{});

  PatternTestCase test_case = GetParam();

  for (const std::string& sample : test_case.positive_samples) {
    Validate(test_case.pattern_name, test_case.language,
             /*is_positive_sample=*/true, sample);
  }

  for (const std::string& sample : test_case.negative_samples) {
    Validate(test_case.pattern_name, test_case.language,
             /*is_positive_sample=*/false, sample);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DefaultRegExPatternsTest,
    testing::Values(
        PatternTestCase{
            .pattern_name = "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR",
            .language = "en",
            .positive_samples =
                {"mm / yy", "mm/ yy", "mm /yy", "mm/yy", "mm - yy", "mm- yy",
                 "mm -yy", "mm-yy", "mmyy",
                 // Complex two year cases
                 "Expiration Date (MM / YY)", "Expiration Date (MM/YY)",
                 "Expiration Date (MM - YY)", "Expiration Date (MM-YY)",
                 "Expiration Date MM / YY", "Expiration Date MM/YY",
                 "Expiration Date MM - YY", "Expiration Date MM-YY",
                 "expiration date yy", "Exp Date     (MM / YY)"},
            .negative_samples =
                {"", "Look, ma' -- an invalid string!", "mmfavouritewordyy",
                 "mm a yy", "mm a yyyy",
                 // Simple four year cases
                 "mm / yyyy", "mm/ yyyy", "mm /yyyy", "mm/yyyy", "mm - yyyy",
                 "mm- yyyy", "mm -yyyy", "mm-yyyy", "mmyyyy",
                 // Complex four year cases
                 "Expiration Date (MM / YYYY)", "Expiration Date (MM/YYYY)",
                 "Expiration Date (MM - YYYY)", "Expiration Date (MM-YYYY)",
                 "Expiration Date MM / YYYY", "Expiration Date MM/YYYY",
                 "Expiration Date MM - YYYY", "Expiration Date MM-YYYY",
                 "expiration date yyyy", "Exp Date     (MM / YYYY)"}},
        PatternTestCase{
            .pattern_name = "CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR",
            .language = "en",
            .positive_samples =
                {// Simple four year cases
                 "mm / yyyy", "mm/ yyyy", "mm /yyyy", "mm/yyyy", "mm - yyyy",
                 "mm- yyyy", "mm -yyyy", "mm-yyyy", "mmyyyy",
                 // Complex four year cases
                 "Expiration Date (MM / YYYY)", "Expiration Date (MM/YYYY)",
                 "Expiration Date (MM - YYYY)", "Expiration Date (MM-YYYY)",
                 "Expiration Date MM / YYYY", "Expiration Date MM/YYYY",
                 "Expiration Date MM - YYYY", "Expiration Date MM-YYYY",
                 "expiration date yyyy", "Exp Date     (MM / YYYY)"},
            .negative_samples =
                {"", "Look, ma' -- an invalid string!", "mmfavouritewordyy",
                 "mm a yy", "mm a yyyy",
                 // Simple two year cases
                 "mm / yy", "mm/ yy", "mm /yy", "mm/yy", "mm - yy", "mm- yy",
                 "mm -yy", "mm-yy", "mmyy",
                 // Complex two year cases
                 "Expiration Date (MM / YY)", "Expiration Date (MM/YY)",
                 "Expiration Date (MM - YY)", "Expiration Date (MM-YY)",
                 "Expiration Date MM / YY", "Expiration Date MM/YY",
                 "Expiration Date MM - YY", "Expiration Date MM-YY",
                 "expiration date yy", "Exp Date     (MM / YY)"}},
        PatternTestCase{.pattern_name = "ZIP_CODE",
                        .language = "en",
                        .positive_samples = {"Zip code", "postal code"},
                        .negative_samples =
                            {// Not matching for "en" language:
                             "postleitzahl",
                             // Not referring to a ZIP code:
                             "Supported file formats: .docx, .rar, .zip."}},
        PatternTestCase{.pattern_name = "ZIP_CODE",
                        .language = "de",
                        .positive_samples = {// Inherited from "en":
                                             "Zip code", "postal code",
                                             // Specifically added for "de":
                                             "postleitzahl"}}));

}  // namespace autofill
