// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_platform.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/win/windows_version.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SpellcheckPlatformWinTest : public testing::Test {
 public:
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

 protected:
  bool callback_finished_ = false;

  bool set_language_result_;
  std::vector<SpellCheckResult> spell_check_results_;
  base::OnceClosure quit_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(SpellcheckPlatformWinTest, SpellCheckSuggestions_EN_US) {
  static const struct {
    const char* input;           // A string to be tested.
    const char* suggested_word;  // A suggested word that should occur.
  } kTestCases[] = {
      {"absense", "absence"},    {"becomeing", "becoming"},
      {"cieling", "ceiling"},    {"definate", "definite"},
      {"eigth", "eight"},        {"exellent", "excellent"},
      {"finaly", "finally"},     {"garantee", "guarantee"},
      {"humerous", "humorous"},  {"imediately", "immediately"},
      {"jellous", "jealous"},    {"knowlege", "knowledge"},
      {"lenght", "length"},      {"manuever", "maneuver"},
      {"naturaly", "naturally"}, {"ommision", "omission"},
  };

  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(spellcheck::kWinUseBrowserSpellChecker);

  spellcheck_platform::SetLanguage(
      "en-US",
      base::BindOnce(&SpellcheckPlatformWinTest::SetLanguageCompletionCallback,
                     base::Unretained(this)));

  RunUntilResultReceived();

  for (const auto& test_case : kTestCases) {
    const base::string16 word(base::ASCIIToUTF16(test_case.input));

    // Check if the suggested words occur.
    spellcheck_platform::RequestTextCheck(
        1, word,
        base::BindOnce(&SpellcheckPlatformWinTest::TextCheckCompletionCallback,
                       base::Unretained(this)));
    RunUntilResultReceived();

    ASSERT_EQ(1u, spell_check_results_.size());

    const std::vector<base::string16>& suggestions =
        spell_check_results_.front().replacements;
    const base::string16 suggested_word(
        base::ASCIIToUTF16(test_case.suggested_word));
    auto position =
        std::find_if(suggestions.begin(), suggestions.end(),
                     [&](const base::string16& suggestion) {
                       return suggestion.compare(suggested_word) == 0;
                     });

    ASSERT_NE(suggestions.end(), position);
  }
}

}  // namespace
