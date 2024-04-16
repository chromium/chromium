// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

#include <stddef.h>
#include <ostream>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "components/spellcheck/browser/windows_spell_checker.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct RequestTextCheckTestCase {
  const char* text_to_check;
  const char* expected_suggestion;
};

std::ostream& operator<<(std::ostream& out,
                         const RequestTextCheckTestCase& test_case) {
  out << "text_to_check=" << test_case.text_to_check
      << ", expected_suggestion=" << test_case.expected_suggestion;
  return out;
}

class WindowsSpellCheckerTest : public testing::Test {
 public:
  WindowsSpellCheckerTest() {
    // The WindowsSpellchecker object can be created even on Windows versions
    // that don't support platform spellchecking. However, the spellcheck
    // factory won't be instantiated and the result returned in the
    // CreateSpellChecker callback will be false.
    win_spell_checker_ = std::make_unique<WindowsSpellChecker>(
        base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()}));

    win_spell_checker_->CreateSpellChecker(
        "en-US",
        base::BindOnce(&WindowsSpellCheckerTest::SetLanguageCompletionCallback,
                       base::Unretained(this)));

    RunUntilResultReceived();
  }

  void RunUntilResultReceived() {
    if (callback_finished_)
      return;
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();

    // reset status
    callback_finished_ = false;
  }

  void SetLanguageCompletionCallback(bool result) {
    set_language_result_ = result;
    callback_finished_ = true;
    if (quit_)
      std::move(quit_).Run();
  }

  void TextCheckCompletionCallback(
      const std::vector<SpellCheckResult>& results) {
    callback_finished_ = true;
    spell_check_results_ = results;
    if (quit_)
      std::move(quit_).Run();
  }

  void PerLanguageSuggestionsCompletionCallback(
      const spellcheck::PerLanguageSuggestions& suggestions) {
    callback_finished_ = true;
    per_language_suggestions_ = suggestions;
    if (quit_)
      std::move(quit_).Run();
  }

  void RetrieveSpellcheckLanguagesCompletionCallback(
      const std::vector<std::string>& spellcheck_languages) {
    callback_finished_ = true;
    spellcheck_languages_ = spellcheck_languages;
    DVLOG(2) << "RetrieveSpellcheckLanguagesCompletionCallback: Dictionary "
                "found for following language tags: "
             << base::JoinString(spellcheck_languages_, ", ");
    if (quit_)
      std::move(quit_).Run();
  }

 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        spellcheck::kWinRetrieveSuggestionsOnlyOnDemand);
  }

  void RunRequestTextCheckTest(const RequestTextCheckTestCase& test_case);

  std::unique_ptr<WindowsSpellChecker> win_spell_checker_;

  bool callback_finished_ = false;
  base::OnceClosure quit_;
  base::test::ScopedFeatureList feature_list_;

  bool set_language_result_;
  std::vector<SpellCheckResult> spell_check_results_;
  spellcheck::PerLanguageSuggestions per_language_suggestions_;
  std::vector<std::string> spellcheck_languages_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

void WindowsSpellCheckerTest::RunRequestTextCheckTest(
    const RequestTextCheckTestCase& test_case) {
  ASSERT_TRUE(set_language_result_);

  const std::u16string word(base::ASCIIToUTF16(test_case.text_to_check));

  // Check if the suggested words occur.
  win_spell_checker_->RequestTextCheck(
      1, word,
      base::BindOnce(&WindowsSpellCheckerTest::TextCheckCompletionCallback,
                     base::Unretained(this)));
  RunUntilResultReceived();

  ASSERT_EQ(1u, spell_check_results_.size())
      << "RequestTextCheck: Wrong number of results";

  const std::vector<std::u16string>& suggestions =
      spell_check_results_.front().replacements;
  if (base::FeatureList::IsEnabled(
          spellcheck::kWinRetrieveSuggestionsOnlyOnDemand)) {
    // RequestTextCheck should return no suggestions.
    ASSERT_TRUE(suggestions.empty())
        << "RequestTextCheck: No suggestions are expected";
  } else {
    const std::u16string suggested_word(
        base::ASCIIToUTF16(test_case.expected_suggestion));
    ASSERT_TRUE(base::ranges::any_of(suggestions, [&](const std::u16string&
                                                          suggestion) {
      return suggestion.compare(suggested_word) == 0;
    })) << "RequestTextCheck: Expected suggestion not found";
  }
}

static const RequestTextCheckTestCase kRequestTextCheckTestCases[] = {
    {"absense", "absence"},    {"becomeing", "becoming"},
    {"cieling", "ceiling"},    {"definate", "definite"},
    {"eigth", "eight"},        {"exellent", "excellent"},
    {"finaly", "finally"},     {"garantee", "guarantee"},
    {"humerous", "humorous"},  {"imediately", "immediately"},
    {"jellous", "jealous"},    {"knowlege", "knowledge"},
    {"lenght", "length"},      {"manuever", "maneuver"},
    {"naturaly", "naturally"}, {"ommision", "omission"},
};

class WindowsSpellCheckerRequestTextCheckTest
    : public WindowsSpellCheckerTest,
      public testing::WithParamInterface<RequestTextCheckTestCase> {};

INSTANTIATE_TEST_SUITE_P(TestCases,
                         WindowsSpellCheckerRequestTextCheckTest,
                         testing::ValuesIn(kRequestTextCheckTestCases));

TEST_P(WindowsSpellCheckerRequestTextCheckTest, RequestTextCheck) {
  RunRequestTextCheckTest(GetParam());
}

class WindowsSpellCheckerRequestTextCheckWithSuggestionsTest
    : public WindowsSpellCheckerRequestTextCheckTest {
 protected:
  void SetUp() override {
    // Want to maintain test coverage for requesting suggestions on call to
    // RequestTextCheck.
    feature_list_.InitAndDisableFeature(
        spellcheck::kWinRetrieveSuggestionsOnlyOnDemand);
  }
};

INSTANTIATE_TEST_SUITE_P(TestCases,
                         WindowsSpellCheckerRequestTextCheckWithSuggestionsTest,
                         testing::ValuesIn(kRequestTextCheckTestCases));

TEST_P(WindowsSpellCheckerRequestTextCheckWithSuggestionsTest,
       RequestTextCheck) {
  // TODO(crbug.com/41485814): Remove once Windows fixes spellcheck.
#if defined(ARCH_CPU_ARM64)
  const char* text_to_check = GetParam().text_to_check;
  if (text_to_check == kRequestTextCheckTestCases[1].text_to_check ||
      text_to_check == kRequestTextCheckTestCases[6].text_to_check ||
      text_to_check == kRequestTextCheckTestCases[10].text_to_check) {
    GTEST_SKIP() << "Newest spell checker drop on Arm64 is broken for several "
                    "test cases";
  }
#endif  // defined(ARCH_CPU_ARM64)
  RunRequestTextCheckTest(GetParam());
}

TEST_F(WindowsSpellCheckerTest, RetrieveSpellcheckLanguages) {
  // Test retrieval of real dictionary on system (useful for debug logging
  // other registered dictionaries).
  win_spell_checker_->RetrieveSpellcheckLanguages(base::BindOnce(
      &WindowsSpellCheckerTest::RetrieveSpellcheckLanguagesCompletionCallback,
      base::Unretained(this)));

  RunUntilResultReceived();

  ASSERT_LE(1u, spellcheck_languages_.size());
  ASSERT_TRUE(base::Contains(spellcheck_languages_, "en-US"));
}

TEST_F(WindowsSpellCheckerTest, RetrieveSpellcheckLanguagesFakeDictionaries) {
  // Test retrieval of fake dictionaries added using
  // AddSpellcheckLanguagesForTesting. If fake dictionaries are used,
  // instantiation of the spellchecker factory is not required for
  // RetrieveSpellcheckLanguages, so the test should pass even on Windows
  // versions that don't support platform spellchecking.
  std::vector<std::string> spellcheck_languages_for_testing = {
      "ar-SA", "es-419", "fr-CA"};
  win_spell_checker_->AddSpellcheckLanguagesForTesting(
      spellcheck_languages_for_testing);

  DVLOG(2) << "Calling RetrieveSpellcheckLanguages after fake dictionaries "
              "added using AddSpellcheckLanguagesForTesting...";
  win_spell_checker_->RetrieveSpellcheckLanguages(base::BindOnce(
      &WindowsSpellCheckerTest::RetrieveSpellcheckLanguagesCompletionCallback,
      base::Unretained(this)));

  RunUntilResultReceived();

  ASSERT_EQ(spellcheck_languages_for_testing, spellcheck_languages_);
}

TEST_F(WindowsSpellCheckerTest, GetPerLanguageSuggestions) {
  ASSERT_TRUE(set_language_result_);

  win_spell_checker_->GetPerLanguageSuggestions(
      u"tihs",
      base::BindOnce(
          &WindowsSpellCheckerTest::PerLanguageSuggestionsCompletionCallback,
          base::Unretained(this)));
  RunUntilResultReceived();

  ASSERT_EQ(per_language_suggestions_.size(), 1u);
  ASSERT_GT(per_language_suggestions_[0].size(), 0u);
}

}  // namespace
