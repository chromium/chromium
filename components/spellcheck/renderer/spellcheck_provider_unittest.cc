// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_provider_test.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_text_checking_result.h"
#include "third_party/blink/public/web/web_text_decoration_type.h"

namespace {

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
struct HybridSpellCheckTestCase {
  size_t language_count;
  size_t enabled_language_count;
  size_t expected_spelling_service_calls_count;
  size_t expected_text_check_requests_count;
};

struct CombineSpellCheckResultsTestCase {
  std::string browser_locale;
  std::string renderer_locale;
  const wchar_t* text;
  std::vector<SpellCheckResult> browser_results;
  bool use_spelling_service;
  blink::WebVector<blink::WebTextCheckingResult> expected_results;
};

std::ostream& operator<<(std::ostream& out,
                         const CombineSpellCheckResultsTestCase& test_case) {
  out << "browser_locale=" << test_case.browser_locale
      << ", renderer_locale=" << test_case.renderer_locale << ", text=\""
      << test_case.text
      << "\", use_spelling_service=" << test_case.use_spelling_service;
  return out;
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

class SpellCheckProviderCacheTest : public SpellCheckProviderTest {
 protected:
  void UpdateCustomDictionary() {
    SpellCheck* spellcheck = provider_.spellcheck();
    EXPECT_NE(spellcheck, nullptr);
    // Skip adding friend class - use public CustomDictionaryChanged from
    // |spellcheck::mojom::SpellChecker|
    static_cast<spellcheck::mojom::SpellChecker*>(spellcheck)
        ->CustomDictionaryChanged({}, {});
  }
};

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
// Test fixture for testing hybrid check cases.
class HybridSpellCheckTest
    : public testing::TestWithParam<HybridSpellCheckTestCase> {
 public:
  HybridSpellCheckTest() : provider_(&embedder_provider_) {}
  ~HybridSpellCheckTest() override {}

  void SetUp() override {
    // Don't delay initialization of the SpellcheckService on browser launch.
    feature_list_.InitAndDisableFeature(
        spellcheck::kWinDelaySpellcheckServiceInit);
  }

  void RunShouldUseBrowserSpellCheckOnlyWhenNeededTest();

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  spellcheck::EmptyLocalInterfaceProvider embedder_provider_;
  TestingSpellCheckProvider provider_;
};

// Test fixture for testing hybrid check cases with delayed initialization of
// the spellcheck service.
class HybridSpellCheckTestDelayInit : public HybridSpellCheckTest {
 public:
  HybridSpellCheckTestDelayInit() = default;

  void SetUp() override {
    // Don't initialize the SpellcheckService on browser launch.
    feature_list_.InitAndEnableFeature(
        spellcheck::kWinDelaySpellcheckServiceInit);
  }
};

// Test fixture for testing combining results from both the native spell checker
// and Hunspell.
class CombineSpellCheckResultsTest
    : public testing::TestWithParam<CombineSpellCheckResultsTestCase> {
 public:
  CombineSpellCheckResultsTest() : provider_(&embedder_provider_) {}
  ~CombineSpellCheckResultsTest() override {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  spellcheck::EmptyLocalInterfaceProvider embedder_provider_;
  TestingSpellCheckProvider provider_;
};
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

TEST_F(SpellCheckProviderCacheTest, SubstringWithoutMisspellings) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(u"This is a test", last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(u"This is a", &completion));
  EXPECT_EQ(result.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, SubstringWithMisspellings) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  std::vector<blink::WebTextCheckingResult> results;
  results.push_back(
      blink::WebTextCheckingResult(blink::kWebTextDecorationTypeSpelling, 5, 3,
                                   std::vector<blink::WebString>({"isq"})));
  last_results.Assign(results);
  provider_.SetLastResults(u"This isq a test", last_results);
  EXPECT_TRUE(provider_.SatisfyRequestFromCache(u"This isq a", &completion));
  EXPECT_EQ(result.completion_count_, 1U);
}

TEST_F(SpellCheckProviderCacheTest, ShorterTextNotSubstring) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(u"This is a test", last_results);
  EXPECT_FALSE(provider_.SatisfyRequestFromCache(u"That is a", &completion));
  EXPECT_EQ(result.completion_count_, 0U);
}

TEST_F(SpellCheckProviderCacheTest, ResetCacheOnCustomDictionaryUpdate) {
  FakeTextCheckingResult result;
  FakeTextCheckingCompletion completion(&result);

  blink::WebVector<blink::WebTextCheckingResult> last_results;
  provider_.SetLastResults(u"This is a test", last_results);

  UpdateCustomDictionary();

  EXPECT_FALSE(provider_.SatisfyRequestFromCache(u"This is a", &completion));
  EXPECT_EQ(result.completion_count_, 0U);
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
// Tests that the SpellCheckProvider does not call into the native spell checker
// on Windows when the native spell checker flags are disabled.
TEST_F(SpellCheckProviderTest, ShouldNotUseBrowserSpellCheck) {
  spellcheck::ScopedDisableBrowserSpellCheckerForTesting
      disable_browser_spell_checker;

  FakeTextCheckingResult completion;
  std::u16string text = u"This is a test";
  provider_.RequestTextChecking(
      text, std::make_unique<FakeTextCheckingCompletion>(&completion));

  EXPECT_EQ(provider_.spelling_service_call_count_, 1U);
  EXPECT_EQ(provider_.text_check_requests_.size(), 0U);
  EXPECT_EQ(completion.completion_count_, 1U);
  EXPECT_EQ(completion.cancellation_count_, 0U);
}

static const HybridSpellCheckTestCase kSpellCheckProviderHybridTestsParams[] = {
    // No languages - should go straight to completion
    HybridSpellCheckTestCase{0U, 0U, 0U, 0U},
    // 1 disabled language - should go to browser
    HybridSpellCheckTestCase{1U, 0U, 0U, 1U},
    // 1 enabled language - should skip browser
    HybridSpellCheckTestCase{1U, 1U, 1U, 0U},
    // 1 disabled language, 1 enabled - should should go to browser
    HybridSpellCheckTestCase{2U, 1U, 0U, 1U},
    // 3 enabled languages - should skip browser
    HybridSpellCheckTestCase{3U, 3U, 1U, 0U},
    // 3 disabled languages - should go to browser
    HybridSpellCheckTestCase{3U, 0U, 0U, 1U},
    // 3 disabled languages, 3 enabled - should go to browser
    HybridSpellCheckTestCase{6U, 3U, 0U, 1U}};

// Tests that the SpellCheckProvider calls into the native spell checker only
// when needed.
INSTANTIATE_TEST_SUITE_P(
    SpellCheckProviderHybridTests,
    HybridSpellCheckTest,
    testing::ValuesIn(kSpellCheckProviderHybridTestsParams));

TEST_P(HybridSpellCheckTest, ShouldUseBrowserSpellCheckOnlyWhenNeeded) {
  RunShouldUseBrowserSpellCheckOnlyWhenNeededTest();
}

void HybridSpellCheckTest::RunShouldUseBrowserSpellCheckOnlyWhenNeededTest() {
  const auto& test_case = GetParam();

  FakeTextCheckingResult completion;
  provider_.spellcheck()->SetFakeLanguageCounts(
      test_case.language_count, test_case.enabled_language_count);
  provider_.RequestTextChecking(
      u"This is a test",
      std::make_unique<FakeTextCheckingCompletion>(&completion));

  EXPECT_EQ(provider_.spelling_service_call_count_,
            test_case.expected_spelling_service_calls_count);
  EXPECT_EQ(provider_.text_check_requests_.size(),
            test_case.expected_text_check_requests_count);
  EXPECT_EQ(completion.completion_count_,
            test_case.expected_text_check_requests_count > 0u ? 0u : 1u);
  EXPECT_EQ(completion.cancellation_count_, 0U);
}

// Tests that the SpellCheckProvider calls into the native spell checker only
// when needed when the code path through
// SpellCheckProvider::RequestTextChecking is that used when the spellcheck
// service is initialized on demand.
INSTANTIATE_TEST_SUITE_P(
    SpellCheckProviderHybridTests,
    HybridSpellCheckTestDelayInit,
    testing::ValuesIn(kSpellCheckProviderHybridTestsParams));

TEST_P(HybridSpellCheckTestDelayInit,
       ShouldUseBrowserSpellCheckOnlyWhenNeeded) {
  RunShouldUseBrowserSpellCheckOnlyWhenNeededTest();
}

// Tests that the SpellCheckProvider can correctly combine results from the
// native spell checker and Hunspell.
INSTANTIATE_TEST_SUITE_P(
    SpellCheckProviderCombineResultsTests,
    CombineSpellCheckResultsTest,
    testing::Values(
        // Browser-only check, no browser results
        CombineSpellCheckResultsTestCase{"en-US",
                                         "",
                                         L"This has no misspellings",
                                         {},
                                         false,
                                         {}},

        // Browser-only check, no spelling service, browser results
        CombineSpellCheckResultsTestCase{
            "en-US",
            "",
            L"Tihs has soem misspellings",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              0,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              9,
                              4,
                              {std::u16string(u"foo")})},
            false,
            blink::WebVector<blink::WebTextCheckingResult>(
                {blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     0,
                     4),
                 blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     9,
                     4)})},

        // Browser-only check, no spelling service, browser results, some words
        // in character sets with no dictionaries enabled.
        CombineSpellCheckResultsTestCase{
            "en-US",
            "",
            L"Tihs has soem \x3053\x3093\x306B\x3061\x306F \x4F60\x597D "
            L"\xC548\xB155\xD558\xC138\xC694 "
            L"\x0930\x093E\x091C\x0927\x093E\x0928 words in different "
            L"character sets "
            L"(Japanese, Chinese, Korean, Hindi)",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              0,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              9,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              14,
                              5,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              20,
                              2,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              23,
                              5,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              29,
                              6,
                              {std::u16string(u"foo")})},
            false,
            blink::WebVector<blink::WebTextCheckingResult>(
                {blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     0,
                     4),
                 blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     9,
                     4)})},

        // Browser-only check, spelling service, spelling-only browser results
        CombineSpellCheckResultsTestCase{
            "en-US",
            "",
            L"Tihs has soem misspellings",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              0,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              9,
                              4,
                              {std::u16string(u"foo")})},
            true,
            blink::WebVector<blink::WebTextCheckingResult>(
                {blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     0,
                     4),
                 blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     9,
                     4)})},

        // Browser-only check, spelling service, spelling and grammar browser
        // results
        CombineSpellCheckResultsTestCase{
            "en-US",
            "",
            L"Tihs has soem misspellings",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              0,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::GRAMMAR,
                              9,
                              4,
                              {std::u16string(u"foo")})},
            true,
            blink::WebVector<blink::WebTextCheckingResult>(
                {blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     0,
                     4),
                 blink::WebTextCheckingResult(blink::WebTextDecorationType::
                                                  kWebTextDecorationTypeGrammar,
                                              9,
                                              4)})},

        // Hybrid check, no browser results
        CombineSpellCheckResultsTestCase{"en-US",
                                         "en-US",
                                         L"This has no misspellings",
                                         {},
                                         false,
                                         {}},

        // Hybrid check, no spelling service, browser results that all coincide
        // with Hunspell results
        CombineSpellCheckResultsTestCase{
            "en-US",
            "en-US",
            L"Tihs has soem misspellings",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              0,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              9,
                              4,
                              {std::u16string(u"foo")})},
            false,
            blink::WebVector<blink::WebTextCheckingResult>(
                {blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     0,
                     4),
                 blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     9,
                     4)})},

        // Hybrid check, no spelling service, browser results that partially
        // coincide with Hunspell results
        CombineSpellCheckResultsTestCase{
            "en-US",
            "en-US",
            L"Tihs has soem misspellings",
            {
                SpellCheckResult(SpellCheckResult::SPELLING,
                                 0,
                                 4,
                                 {std::u16string(u"foo")}),
                SpellCheckResult(SpellCheckResult::SPELLING,
                                 5,
                                 3,
                                 {std::u16string(u"foo")}),
                SpellCheckResult(SpellCheckResult::SPELLING,
                                 9,
                                 4,
                                 {std::u16string(u"foo")}),
                SpellCheckResult(SpellCheckResult::SPELLING,
                                 14,
                                 12,
                                 {std::u16string(u"foo")}),
            },
            false,
            blink::WebVector<blink::WebTextCheckingResult>(
                {blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     0,
                     4),
                 blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     9,
                     4)})},

        // Hybrid check, no spelling service, browser results that don't
        // coincide with Hunspell results
        CombineSpellCheckResultsTestCase{
            "en-US",
            "en-US",
            L"Tihs has soem misspellings",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              5,
                              3,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              14,
                              12,
                              {std::u16string(u"foo")})},
            false,
            blink::WebVector<blink::WebTextCheckingResult>()},

        // Hybrid check, no spelling service, browser results with some that
        // that are in character set that does not have dictionary support (so
        // (are considered spelled correctly)
        CombineSpellCheckResultsTestCase{
            "en-US",
            "fr",
            L"Tihs mot is misspelled in Russian: "
            L"\x043C\x0438\x0440\x0432\x043E\x0439",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              0,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              5,
                              3,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              35,
                              6,
                              {std::u16string(u"foo")})},
            false,
            blink::WebVector<blink::WebTextCheckingResult>(
                std::vector<blink::WebTextCheckingResult>(
                    {blink::WebTextCheckingResult(
                        blink::WebTextDecorationType::
                            kWebTextDecorationTypeSpelling,
                        0,
                        4)}))},

        // Hybrid check, no spelling service, text with some words in a
        // character set that only has a Hunspell dictionary enabled.
        CombineSpellCheckResultsTestCase{
            "en-US",
            "ru",
            L"Tihs \x0432\x0441\x0435\x0445 is misspelled in Russian: "
            L"\x043C\x0438\x0440\x0432\x043E\x0439",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              0,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              5,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              36,
                              6,
                              {std::u16string(u"foo")})},
            false,
            blink::WebVector<blink::WebTextCheckingResult>(
                {blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     0,
                     4),
                 blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     36,
                     6)})},

        // Hybrid check, spelling service, spelling and grammar browser results
        // that all coincide with Hunspell results
        CombineSpellCheckResultsTestCase{
            "en-US",
            "en-US",
            L"Tihs has soem misspellings",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              0,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::GRAMMAR,
                              9,
                              4,
                              {std::u16string(u"foo")})},
            true,
            blink::WebVector<blink::WebTextCheckingResult>(
                {blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     0,
                     4),
                 blink::WebTextCheckingResult(blink::WebTextDecorationType::
                                                  kWebTextDecorationTypeGrammar,
                                              9,
                                              4)})},

        // Hybrid check, spelling service, spelling and grammar browser results
        // but some of the spelling results are correctly spelled in Hunspell
        // locales
        CombineSpellCheckResultsTestCase{
            "en-US",
            "en-US",
            L"This has soem misspellings",
            {SpellCheckResult(SpellCheckResult::SPELLING,
                              0,
                              4,
                              {std::u16string(u"foo")}),
             SpellCheckResult(SpellCheckResult::SPELLING,
                              9,
                              4,
                              {std::u16string(u"foo")})},
            true,
            blink::WebVector<blink::WebTextCheckingResult>(
                {blink::WebTextCheckingResult(blink::WebTextDecorationType::
                                                  kWebTextDecorationTypeGrammar,
                                              0,
                                              4),
                 blink::WebTextCheckingResult(
                     blink::WebTextDecorationType::
                         kWebTextDecorationTypeSpelling,
                     9,
                     4)})}));

TEST_P(CombineSpellCheckResultsTest, ShouldCorrectlyCombineHybridResults) {
  const auto& test_case = GetParam();
  const bool has_browser_check = !test_case.browser_locale.empty();
  const bool has_renderer_check = !test_case.renderer_locale.empty();

  if (has_browser_check) {
    provider_.spellcheck()->InitializeSpellCheckForLocale(
        test_case.browser_locale, /*use_hunspell*/ false);
  }

  if (has_renderer_check) {
    provider_.spellcheck()->InitializeSpellCheckForLocale(
        test_case.renderer_locale, /*use_hunspell*/ true);
  }

  if (test_case.use_spelling_service) {
    for (auto& result : test_case.browser_results) {
      const_cast<SpellCheckResult&>(result).spelling_service_used = true;
    }
  }

  FakeTextCheckingResult completion;
  SpellCheckProvider::HybridSpellCheckRequestInfo request_info = {
      /*used_hunspell=*/has_renderer_check,
      /*used_native=*/has_browser_check, base::TimeTicks::Now()};

  int check_id = provider_.AddCompletionForTest(
      std::make_unique<FakeTextCheckingCompletion>(&completion), request_info);
  provider_.OnRespondTextCheck(check_id, base::WideToUTF16(test_case.text),
                               test_case.browser_results);

  // Should have called the completion callback without cancellation, and should
  // not have initiated more requests.
  ASSERT_EQ(completion.completion_count_, 1u);
  ASSERT_EQ(completion.cancellation_count_, 0U);
  ASSERT_EQ(provider_.spelling_service_call_count_, 0u);
  ASSERT_EQ(provider_.text_check_requests_.size(), 0u);

  // Should have the expected spell check results.
  ASSERT_EQ(test_case.expected_results.size(), completion.results_.size());

  for (size_t i = 0; i < test_case.expected_results.size(); ++i) {
    const auto& expected = test_case.expected_results[i];
    const auto& actual = completion.results_[i];

    ASSERT_EQ(expected.decoration, actual.decoration);
    ASSERT_EQ(expected.location, actual.location);
    ASSERT_EQ(expected.length, actual.length);

    if (has_renderer_check) {
      // Replacements should have been purged so Blink doesn't cache incomplete
      // suggestions.
      ASSERT_EQ(actual.replacements.size(), 0u);
    } else {
      // Replacements should have been kept since they are complete.
      ASSERT_GT(actual.replacements.size(), 0u);
    }
  }
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

}  // namespace
