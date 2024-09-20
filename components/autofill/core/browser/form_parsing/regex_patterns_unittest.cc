// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/regex_patterns.h"

// Keep these tests in sync with
// components/autofill/core/browser/autofill_regexes_unittest.cc.
// Only these tests will be kept once the pattern provider launches.

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/ranges/ranges.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns_inl.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

bool operator==(MatchPatternRef a, MatchPatternRef b) {
  return test_api(a).is_supplementary() == test_api(b).is_supplementary() &&
         test_api(a).index() == test_api(b).index();
}

bool operator!=(MatchPatternRef a, MatchPatternRef b) {
  return !(a == b);
}

void PrintTo(MatchPatternRef p, std::ostream* os) {
  *os << "MatchPatternRef(" << test_api(p).is_supplementary() << ","
      << test_api(p).index() << ")";
}

namespace {

using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

auto Matches(std::u16string_view regex) {
  icu::RegexPattern regex_pattern = *CompileRegex(regex);
  return ::testing::Truly(
      [regex_pattern = std::move(regex_pattern)](std::string_view actual) {
        return MatchesRegex(base::UTF8ToUTF16(actual), regex_pattern);
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

// The parameter is the PatternFile to pass to GetMatchPatterns().
class RegexPatternsTest : public testing::TestWithParam<PatternFile> {
 public:
  PatternFile pattern_file() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(RegexPatternsTest,
                         RegexPatternsTest,
                         ::testing::Values(
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
                             PatternFile::kLegacy
#else
                             PatternFile::kDefault
#endif
                             ));

// The parameter is the index of a MatchPatternRef.
class MatchPatternRefInternalsTest
    : public ::testing::TestWithParam<MatchPatternRefTestApi::UnderlyingType> {
 public:
  MatchPatternRefTestApi::UnderlyingType index() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(RegexPatternsTest,
                         MatchPatternRefInternalsTest,
                         ::testing::Values(0, 1, 2, 123, 1000, 2000));

// Tests MatchPatternRef's index() and is_supplementary().
TEST_P(MatchPatternRefInternalsTest, MatchPatternRef) {
  MatchPatternRef a = MakeMatchPatternRef(false, index());
  MatchPatternRef b = MakeMatchPatternRef(true, index());
  EXPECT_EQ(a, a);
  EXPECT_EQ(b, b);
  EXPECT_NE(a, b);
  EXPECT_EQ(test_api(a).index(), index());
  EXPECT_EQ(test_api(b).index(), index());
  EXPECT_FALSE(test_api(a).is_supplementary());
  EXPECT_TRUE(test_api(b).is_supplementary());
}

// Tests MatchPatternRef's dereference operator.
//
// Since we want to test that supplementary patterns only contain
// MatchAttribute::kName, choose `index` such that `kPatterns[0]` contains
// MatchAttribute::kLabel.
TEST_F(RegexPatternsTest, MatchPatternRefDereference) {
  MatchPatternRefTestApi::UnderlyingType index = 0;
  ASSERT_TRUE(
      kPatterns[0].match_field_attributes.contains(MatchAttribute::kLabel));
  MatchPatternRef a = MakeMatchPatternRef(false, index);
  MatchPatternRef b = MakeMatchPatternRef(true, index);
  EXPECT_TRUE((*a).positive_pattern);
  EXPECT_TRUE((*a).negative_pattern);
  EXPECT_EQ((*a).positive_pattern, (*b).positive_pattern);
  EXPECT_EQ((*a).negative_pattern, (*b).negative_pattern);
  EXPECT_EQ((*a).form_control_types, (*b).form_control_types);
  EXPECT_THAT((*a).match_field_attributes, Contains(MatchAttribute::kLabel));
  EXPECT_THAT((*b).match_field_attributes, ElementsAre(MatchAttribute::kName));
  EXPECT_EQ((*a).feature.has_value(), (*b).feature.has_value());
}

TEST_F(RegexPatternsTest, IsSupportedLanguageCode) {
  EXPECT_TRUE(IsSupportedLanguageCode(LanguageCode("en")));
  EXPECT_TRUE(IsSupportedLanguageCode(LanguageCode("de")));
  EXPECT_TRUE(IsSupportedLanguageCode(LanguageCode("fr")));
  EXPECT_TRUE(IsSupportedLanguageCode(LanguageCode("zh-CN")));
  EXPECT_TRUE(IsSupportedLanguageCode(LanguageCode("zh-TW")));
}

// Tests that for a given pattern name, the pseudo-language-code "" contains the
// patterns of all real languages.
TEST_P(RegexPatternsTest, PseudoLanguageIsUnionOfLanguages) {
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
    const auto& patterns =
        GetMatchPatterns(kSomeName, LanguageCode(lang), pattern_file());
    expected.insert(expected.end(), patterns.begin(), patterns.end());
  }
  std::erase_if(expected,
                [](auto p) { return test_api(p).is_supplementary(); });

  EXPECT_THAT(GetMatchPatterns(kSomeName, std::nullopt, pattern_file()),
              UnorderedElementsAreArray(expected));
  EXPECT_THAT(GetMatchPatterns(kSomeName, std::nullopt, pattern_file()),
              Each(Not(IsSupplementary)));
}

// Tests that for a given pattern name, if the language doesn't isn't known we
// use the union of all patterns.
TEST_P(RegexPatternsTest, FallbackToPseudoLanguageIfLanguageDoesNotExist) {
  const std::string kSomeName = "ADDRESS_LINE_1";
  const LanguageCode kNonexistingLanguage("foo");
  EXPECT_THAT(GetMatchPatterns(kSomeName, kNonexistingLanguage, pattern_file()),
              ElementsAreArray(
                  GetMatchPatterns(kSomeName, std::nullopt, pattern_file())));
}

// Tests that for a given pattern name, the non-English languages are
// supplemented with the English patterns.
TEST_P(RegexPatternsTest,
       EnglishPatternsAreAddedToOtherLanguagesAsSupplementaryPatterns) {
  const std::string kSomeName = "ADDRESS_LINE_1";
  auto de_patterns =
      GetMatchPatterns(kSomeName, LanguageCode("de"), pattern_file());
  auto en_patterns =
      GetMatchPatterns(kSomeName, LanguageCode("en"), pattern_file());
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
  // The set of patterns. In non-branded builds, only the default set is
  // supported.
  PatternFile pattern_file;
  // Reference to the pattern name in the resources/regex_patterns.json file.
  const char* pattern_name;
  // Language selector for the pattern, refers to the detected language of a
  // website.
  const char* language = "en";
  // Strings that should be matched by the pattern.
  std::vector<std::string> positive_samples;
  std::vector<std::string> negative_samples;
};

class RegexPatternsTestWithSamples
    : public testing::TestWithParam<PatternTestCase> {};

TEST_P(RegexPatternsTestWithSamples, TestPositiveAndNegativeCases) {
  PatternTestCase test_case = GetParam();

  for (const std::string& sample : test_case.positive_samples) {
    EXPECT_THAT(sample,
                MatchesAny(GetMatchPatterns(test_case.pattern_name,
                                            LanguageCode(test_case.language),
                                            test_case.pattern_file)))
        << "pattern_source=" << static_cast<int>(test_case.pattern_file) << ","
        << "pattern_name=" << test_case.pattern_name << ","
        << "language=" << test_case.language;
  }

  for (const std::string& sample : test_case.negative_samples) {
    EXPECT_THAT(sample,
                Not(MatchesAny(GetMatchPatterns(
                    test_case.pattern_name, LanguageCode(test_case.language),
                    test_case.pattern_file))))
        << "pattern_name=" << test_case.pattern_name << ","
        << "language=" << test_case.language;
  }
}

INSTANTIATE_TEST_SUITE_P(RegexPatternsTest,
                         RegexPatternsTestWithSamples,
                         testing::Values(
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
                             PatternTestCase {
                               .pattern_file = PatternFile::kLegacy,
                               .pattern_name = "PATTERN_SOURCE_DUMMY",
                               .language = "en", .positive_samples = {"legacy"},
                               .negative_samples = {
                                 "default",
                                 "experimental"
                               }
                             }
#else
                             PatternTestCase{
                                 .pattern_file = PatternFile::kDefault,
                                 .pattern_name = "PATTERN_SOURCE_DUMMY",
                                 .language = "en",
                                 .positive_samples = {"default"},
                                 .negative_samples = {"legacy",
                                                      "experimental"}},
                             PatternTestCase{
                                 .pattern_file = PatternFile::kDefault,
                                 .pattern_name =
                                     "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR",
                                 .language = "en",
                                 .positive_samples =
                                     {"mm / yy", "mm/ yy", "mm /yy", "mm/yy",
                                      "mm - yy", "mm- yy", "mm -yy", "mm-yy",
                                      "mmyy",
                                      // Complex two year cases
                                      "Expiration Date (MM / YY)",
                                      "Expiration Date (MM/YY)",
                                      "Expiration Date (MM - YY)",
                                      "Expiration Date (MM-YY)",
                                      "Expiration Date MM / YY",
                                      "Expiration Date MM/YY",
                                      "Expiration Date MM - YY",
                                      "Expiration Date MM-YY",
                                      "expiration date yy",
                                      "Exp Date     (MM / YY)"},
                                 .negative_samples =
                                     {"", "Look, ma' -- an invalid string!",
                                      "mmfavouritewordyy", "mm a yy",
                                      "mm a yyyy",
                                      // Simple four year cases
                                      "mm / yyyy", "mm/ yyyy", "mm /yyyy",
                                      "mm/yyyy", "mm - yyyy", "mm- yyyy",
                                      "mm -yyyy", "mm-yyyy", "mmyyyy",
                                      // Complex four year cases
                                      "Expiration Date (MM / YYYY)",
                                      "Expiration Date (MM/YYYY)",
                                      "Expiration Date (MM - YYYY)",
                                      "Expiration Date (MM-YYYY)",
                                      "Expiration Date MM / YYYY",
                                      "Expiration Date MM/YYYY",
                                      "Expiration Date MM - YYYY",
                                      "Expiration Date MM-YYYY",
                                      "expiration date yyyy",
                                      "Exp Date     (MM / YYYY)"}},
                             PatternTestCase{
                                 .pattern_file = PatternFile::kDefault,
                                 .pattern_name =
                                     "CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR",
                                 .language = "en",
                                 .positive_samples =
                                     {// Simple four year cases
                                      "mm / yyyy", "mm/ yyyy", "mm /yyyy",
                                      "mm/yyyy", "mm - yyyy", "mm- yyyy",
                                      "mm -yyyy", "mm-yyyy", "mmyyyy",
                                      // Complex four year cases
                                      "Expiration Date (MM / YYYY)",
                                      "Expiration Date (MM/YYYY)",
                                      "Expiration Date (MM - YYYY)",
                                      "Expiration Date (MM-YYYY)",
                                      "Expiration Date MM / YYYY",
                                      "Expiration Date MM/YYYY",
                                      "Expiration Date MM - YYYY",
                                      "Expiration Date MM-YYYY",
                                      "expiration date yyyy",
                                      "Exp Date     (MM / YYYY)"},
                                 .negative_samples =
                                     {"", "Look, ma' -- an invalid string!",
                                      "mmfavouritewordyy", "mm a yy",
                                      "mm a yyyy",
                                      // Simple two year cases
                                      "mm / yy", "mm/ yy", "mm /yy", "mm/yy",
                                      "mm - yy", "mm- yy", "mm -yy", "mm-yy",
                                      "mmyy",
                                      // Complex two year cases
                                      "Expiration Date (MM / YY)",
                                      "Expiration Date (MM/YY)",
                                      "Expiration Date (MM - YY)",
                                      "Expiration Date (MM-YY)",
                                      "Expiration Date MM / YY",
                                      "Expiration Date MM/YY",
                                      "Expiration Date MM - YY",
                                      "Expiration Date MM-YY",
                                      "expiration date yy",
                                      "Exp Date     (MM / YY)"}},
                             PatternTestCase{
                                 .pattern_file = PatternFile::kDefault,
                                 .pattern_name = "ZIP_CODE",
                                 .language = "en",
                                 .positive_samples = {"Zip code",
                                                      "postal code"},
                                 .negative_samples =
                                     {// Not matching for "en" language:
                                      "postleitzahl",
                                      // Not referring to a ZIP code:
                                      "Supported file formats: .docx, .rar, "
                                      ".zip."}},
                             PatternTestCase{
                                 .pattern_file = PatternFile::kDefault,
                                 .pattern_name = "ZIP_CODE",
                                 .language = "de",
                                 .positive_samples =
                                     {// Inherited from "en":
                                      "Zip code", "postal code",
                                      // Specifically added for "de":
                                      "postleitzahl"}}
#endif
                             ));

}  // namespace
}  // namespace autofill
