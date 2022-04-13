// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/regex_patterns.h"

// Keep these tests in sync with
// components/autofill/core/browser/autofill_regexes_unittest.cc.
// Only these tests will be kept once the pattern provider launches.

#include <string>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/ranges/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns_inl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

namespace autofill {

class MatchPatternRefTestApi {
 public:
  explicit MatchPatternRefTestApi(MatchPatternRef p) : p_(p) {}

  absl::optional<MatchPatternRef> MakeSupplementary() const {
    if (!(*p_).match_field_attributes.contains(MatchAttribute::kName))
      return absl::nullopt;
    return MatchPatternRef(true, index());
  }

  MatchPatternRef::UnderlyingType is_supplementary() const {
    return p_.is_supplementary();
  }

  MatchPatternRef::UnderlyingType index() const { return p_.index(); }

 private:
  MatchPatternRef p_;
};

namespace {

MatchPatternRefTestApi test_api(MatchPatternRef p) {
  return MatchPatternRefTestApi(p);
}

auto Matches(base::StringPiece16 pattern) {
  return ::testing::Truly([pattern](base::StringPiece actual) {
    return MatchesPattern(base::UTF8ToUTF16(actual), pattern);
  });
}

auto Matches(MatchingPattern pattern) {
  testing::Matcher<std::string> m = Matches(pattern.positive_pattern);
  if (pattern.negative_pattern && *pattern.negative_pattern)
    m = ::testing::AllOf(Not(Matches(pattern.negative_pattern)), m);
  return m;
}

auto Matches(MatchPatternRef pattern_ref) {
  return Matches(*pattern_ref);
}

// Matches if the actual value matches any of the `pattern_refs`.
auto MatchesAny(base::span<const MatchPatternRef> pattern_refs) {
  std::vector<::testing::Matcher<std::string>> matchers;
  for (MatchPatternRef pattern_ref : pattern_refs)
    matchers.push_back(Matches(pattern_ref));
  return ::testing::AnyOfArray(matchers);
}

const auto IsSupplementary = ::testing::Truly(
    [](MatchPatternRef p) { return test_api(p).is_supplementary(); });

bool IsEmpty(const char* s) {
  return s == nullptr || s[0] == '\0';
}

}  // namespace

bool operator==(MatchPatternRef a, MatchPatternRef b) {
  return test_api(a).is_supplementary() == test_api(b).is_supplementary() ||
         test_api(a).index() == test_api(b).index();
}

void PrintTo(MatchPatternRef p, std::ostream* os) {
  *os << "MatchPatternRef(" << test_api(p).is_supplementary() << ","
      << test_api(p).index() << ")";
}

class RegexPatternsTest : public testing::Test {};

// Tests that for a given pattern name, the pseudo-language-code "" contains the
// patterns of all real languages.
TEST_F(RegexPatternsTest, PseudoLanguageIsUnionOfLanguages) {
  const std::string kSomeName = "ADDRESS_LINE_1";
  const base::flat_set<std::string> kLanguagesOfPattern = [&] {
    std::vector<std::string> vec;
    for (const auto& [name_and_lang, patterns] : kPatternMap) {
      const auto& [name, lang] = name_and_lang;
      if (name == kSomeName && !IsEmpty(lang))
        vec.push_back(lang);
    }
    return base::flat_set<std::string>(std::move(vec));
  }();
  ASSERT_THAT(kLanguagesOfPattern, IsSupersetOf({"de", "en", "es", "fr"}));

  // The expected patterns are the patterns of all languages for `kSomeName`.
  std::vector<MatchPatternRef> expected;
  for (const std::string& lang : kLanguagesOfPattern) {
    const auto& patterns = GetMatchPatterns(kSomeName, LanguageCode(lang));
    expected.insert(expected.end(), patterns.begin(), patterns.end());
  }
  base::EraseIf(expected,
                [](auto p) { return test_api(p).is_supplementary(); });

  EXPECT_THAT(GetMatchPatterns(kSomeName, absl::nullopt),
              UnorderedElementsAreArray(expected));
  EXPECT_THAT(GetMatchPatterns(kSomeName, absl::nullopt),
              Each(Not(IsSupplementary)));
}

// Tests that for a given pattern name, if the language doesn't isn't known we
// use the union of all patterns.
TEST_F(RegexPatternsTest, FallbackToPseudoLanguageIfLanguageDoesNotExist) {
  const std::string kSomeName = "ADDRESS_LINE_1";
  const LanguageCode kNonexistingLanguage("foo");
  EXPECT_THAT(GetMatchPatterns(kSomeName, kNonexistingLanguage),
              ElementsAreArray(GetMatchPatterns(kSomeName, absl::nullopt)));
}

// Tests that for a given pattern name, the non-English languages are
// supplemented with the English patterns.
TEST_F(RegexPatternsTest,
       EnglishPatternsAreAddedToOtherLanguagesAsSupplementaryPatterns) {
  const std::string kSomeName = "ADDRESS_LINE_1";
  auto de_patterns = GetMatchPatterns(kSomeName, LanguageCode("de"));
  auto en_patterns = GetMatchPatterns(kSomeName, LanguageCode("en"));
  ASSERT_FALSE(de_patterns.empty());
  ASSERT_FALSE(en_patterns.empty());

  EXPECT_THAT(de_patterns, Not(Each(IsSupplementary)));
  EXPECT_THAT(de_patterns, Not(Each(Not(IsSupplementary))));
  EXPECT_THAT(en_patterns, Each(Not(IsSupplementary)));

  std::vector<MatchPatternRef> expected;
  for (MatchPatternRef p : de_patterns) {
    if (!test_api(p).is_supplementary())
      expected.push_back(p);
  }
  for (MatchPatternRef p : en_patterns) {
    if (auto supplementary_pattern = test_api(p).MakeSupplementary())
      expected.push_back(*supplementary_pattern);
  }
  EXPECT_THAT(de_patterns, UnorderedElementsAreArray(expected));
}

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

class RegexPatternsTestWithSamples
    : public RegexPatternsTest,
      public testing::WithParamInterface<PatternTestCase> {};

TEST_P(RegexPatternsTestWithSamples, TestPositiveAndNegativeCases) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams feature_parameters{
      {features::kAutofillParsingWithLanguageSpecificPatternsParam.name,
       "true"}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillParsingPatternProvider, feature_parameters);

  PatternTestCase test_case = GetParam();

  for (const std::string& sample : test_case.positive_samples) {
    EXPECT_THAT(sample,
                MatchesAny(GetMatchPatterns(test_case.pattern_name,
                                            LanguageCode(test_case.language))))
        << "pattern_name=" << test_case.pattern_name << ","
        << "language=" << test_case.language;
  }

  for (const std::string& sample : test_case.negative_samples) {
    EXPECT_THAT(sample,
                ::testing::Not(MatchesAny(GetMatchPatterns(
                    test_case.pattern_name, LanguageCode(test_case.language)))))
        << "pattern_name=" << test_case.pattern_name << ","
        << "language=" << test_case.language;
  }
}

INSTANTIATE_TEST_SUITE_P(
    RegexPatternsTest,
    RegexPatternsTestWithSamples,
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
