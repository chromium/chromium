// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/empty_local_interface_provider.h"
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_text_checking_result.h"

namespace {

struct SpellcheckTestCase {
  // A string of text for checking.
  const wchar_t* input;
  // The position and the length of the first misspelled word, if any.
  size_t expected_misspelling_start;
  size_t expected_misspelling_length;
};

base::FilePath GetHunspellDirectory() {
  base::FilePath hunspell_directory;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &hunspell_directory))
    return base::FilePath();

  hunspell_directory = hunspell_directory.AppendASCII("third_party");
  hunspell_directory = hunspell_directory.AppendASCII("hunspell_dictionaries");
  return hunspell_directory;
}

}  // namespace

class MultilingualSpellCheckTest : public testing::Test {
 public:
  MultilingualSpellCheckTest() {}

  void ReinitializeSpellCheck(const std::string& unsplit_languages) {
    spellcheck_ = new SpellCheck(&embedder_provider_);
    provider_.reset(
        new TestingSpellCheckProvider(spellcheck_, &embedder_provider_));
    InitializeSpellCheck(unsplit_languages);
  }

  void InitializeSpellCheck(const std::string& unsplit_languages) {
    base::FilePath hunspell_directory = GetHunspellDirectory();
    EXPECT_FALSE(hunspell_directory.empty());
    std::vector<std::string> languages = base::SplitString(
        unsplit_languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    for (const auto& language : languages) {
      base::File file(
          spellcheck::GetVersionedFileName(language, hunspell_directory),
          base::File::FLAG_OPEN | base::File::FLAG_READ);
      spellcheck_->AddSpellcheckLanguage(std::move(file), language);
    }
  }

  ~MultilingualSpellCheckTest() override {}
  TestingSpellCheckProvider* provider() { return provider_.get(); }

 protected:
  void ExpectSpellCheckWordResults(const std::string& languages,
                                   const SpellcheckTestCase* test_cases,
                                   size_t num_test_cases) {
    ReinitializeSpellCheck(languages);

    for (size_t i = 0; i < num_test_cases; ++i) {
      size_t misspelling_start = 0;
      size_t misspelling_length = 0;
      static_cast<blink::WebTextCheckClient*>(provider())
          ->CheckSpelling(blink::WebString::FromUTF16(
                              base::WideToUTF16(test_cases[i].input)),
                          misspelling_start, misspelling_length, nullptr);

      EXPECT_EQ(test_cases[i].expected_misspelling_start, misspelling_start)
          << "Improper misspelling location found with the languages "
          << languages << " when checking \"" << test_cases[i].input << "\".";
      EXPECT_EQ(test_cases[i].expected_misspelling_length, misspelling_length)
          << "Improper misspelling length found with the languages "
          << languages << " when checking \"" << test_cases[i].input << "\".";
    }
  }

  void ExpectSpellCheckParagraphResults(
      const base::string16& input,
      const std::vector<SpellCheckResult>& expected) {
    blink::WebVector<blink::WebTextCheckingResult> results;
    spellcheck_->SpellCheckParagraph(input, &results);

    EXPECT_EQ(expected.size(), results.size());
    size_t size = std::min(results.size(), expected.size());
    for (size_t i = 0; i < size; ++i) {
      EXPECT_EQ(blink::kWebTextDecorationTypeSpelling, results[i].decoration);
      EXPECT_EQ(expected[i].location, results[i].location);
      EXPECT_EQ(expected[i].length, results[i].length);
    }
  }

 private:
  base::test::TaskEnvironment task_environment_;
  spellcheck::EmptyLocalInterfaceProvider embedder_provider_;

  // Owned by |provider_|.
  SpellCheck* spellcheck_;
  std::unique_ptr<TestingSpellCheckProvider> provider_;
};

// Check that a string of different words is properly spellchecked for different
// combinations of different languages.
TEST_F(MultilingualSpellCheckTest, MultilingualSpellCheckWord) {
  static const SpellcheckTestCase kTestCases[] = {
      // An English, Spanish, Russian, and Greek word, all spelled correctly.
      {L"rocket destruyan \x0432\x0441\x0435\x0445 \x03C4\x03B9\x03C2", 0, 0},
      // A misspelled English word.
      {L"rocktt destruyan \x0432\x0441\x0435\x0445 \x03C4\x03B9\x03C2", 0, 6},
      // A misspelled Spanish word.
      {L"rocket destruynn \x0432\x0441\x0435\x0445 \x03C4\x03B9\x03C2", 7, 9},
      // A misspelled Russian word.
      {L"rocket destruyan \x0430\x0430\x0430\x0430 \x03C4\x03B9\x03C2", 17, 4},
      // A misspelled Greek word.
      {L"rocket destruyan \x0432\x0441\x0435\x0445 \x03B1\x03B1\x03B1\x03B1",
       22, 4},
      // An English word, then Russian, and then a misspelled English word.
      {L"rocket \x0432\x0441\x0435\x0445 rocktt", 12, 6},
  };

  // A sorted list of languages. This must start sorted to get all possible
  // permutations.
  std::string languages = "el-GR,en-US,es-ES,ru-RU";
  std::vector<base::StringPiece> permuted_languages = base::SplitStringPiece(
      languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  do {
    std::string reordered_languages = base::JoinString(permuted_languages, ",");
    ExpectSpellCheckWordResults(reordered_languages, kTestCases,
                                base::size(kTestCases));
  } while (std::next_permutation(permuted_languages.begin(),
                                 permuted_languages.end()));
}

TEST_F(MultilingualSpellCheckTest, MultilingualSpellCheckWordEnglishSpanish) {
  static const SpellcheckTestCase kTestCases[] = {
      {L"", 0, 0},
      {L"head hand foot legs arms", 0, 0},
      {L"head hand foot legs arms zzzz", 25, 4},
      {L"head hand zzzz foot legs arms", 10, 4},
      {L"zzzz head hand foot legs arms", 0, 4},
      {L"zzzz head zzzz foot zzzz arms", 0, 4},
      {L"head hand foot arms zzzz zzzz", 20, 4},
      {L"I do not want a monstrous snake near me.", 0, 0},
      {L"zz do not want a monstrous snake near me.", 0, 2},
      {L"I do not want zz monstrous snake near me.", 14, 2},
      {L"I do not want a monstrous zz near me.", 26, 2},
      {L"I do not want a monstrou snake near me.", 16, 8},
      {L"I do not want a monstrous snake near zz.", 37, 2},
      {L"Partially Spanish is very bueno.", 0, 0},
      {L"Sleeping in the biblioteca is good.", 0, 0},
      {L"Hermano is my favorite name.", 0, 0},
      {L"hola hola hola hola hola hola", 0, 0},
      {L"sand hola hola hola hola hola", 0, 0},
      {L"hola sand sand sand sand sand", 0, 0},
      {L"sand sand sand sand sand hola", 0, 0},
      {L"sand hola sand hola sand hola", 0, 0},
      {L"hola sand hola sand hola sand", 0, 0},
      {L"hola:legs", 0, 9},
      {L"legs:hola", 0, 9}};
  ExpectSpellCheckWordResults("en-US,es-ES", kTestCases,
                              base::size(kTestCases));
}

// If there are no spellcheck languages, no text should be marked as misspelled.
TEST_F(MultilingualSpellCheckTest, MultilingualSpellCheckParagraphBlank) {
  ReinitializeSpellCheck(std::string());

  ExpectSpellCheckParagraphResults(
      // English, German, Spanish, and a misspelled word.
      base::UTF8ToUTF16("rocket Schwarzkommando destruyan pcnyhon"),
      std::vector<SpellCheckResult>());
}

// Make sure nothing is considered misspelled when at least one of the selected
// languages determines that a word is correctly spelled.
TEST_F(MultilingualSpellCheckTest, MultilingualSpellCheckParagraphCorrect) {
  ReinitializeSpellCheck("en-US,es-ES,de-DE");

  ExpectSpellCheckParagraphResults(
      // English, German, and Spanish words, all spelled correctly.
      base::UTF8ToUTF16("rocket Schwarzkommando destruyan"),
      std::vector<SpellCheckResult>());
}

// Make sure that all the misspellings in the text are found.
TEST_F(MultilingualSpellCheckTest, MultilingualSpellCheckParagraph) {
  ReinitializeSpellCheck("en-US,es-ES");
  std::vector<SpellCheckResult> expected;
  expected.push_back(SpellCheckResult(SpellCheckResult::SPELLING, 7, 15));
  expected.push_back(SpellCheckResult(SpellCheckResult::SPELLING, 33, 7));

  ExpectSpellCheckParagraphResults(
      // English, German, Spanish, and a misspelled word.
      base::UTF8ToUTF16("rocket Schwarzkommando destruyan pcnyhon"), expected);
}

// Ensure that suggestions are handled properly for multiple languages.
TEST_F(MultilingualSpellCheckTest, MultilingualSpellCheckSuggestions) {
  ReinitializeSpellCheck("en-US,es-ES");
  static const struct {
    // A string of text for checking.
    const wchar_t* input;
    // The position and the length of the first invalid word.
    size_t expected_misspelling_start;
    size_t expected_misspelling_length;
    // A comma separated string of suggested words that should occur, in their
    // expected order.
    const wchar_t* expected_suggestions;
  } kTestCases[] = {
      {L"rocket", 0, 0},
      {L"destruyan", 0, 0},
      {L"rocet", 0, 5, L"rocket,roce,crochet,troce,rocen"},
      {L"jum", 0, 3, L"hum,jun,ju,um,juma"},
      {L"asdne", 0, 5, L"sadness,desasne"},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    blink::WebVector<blink::WebString> suggestions;
    size_t misspelling_start;
    size_t misspelling_length;
    static_cast<blink::WebTextCheckClient*>(provider())
        ->CheckSpelling(
            blink::WebString::FromUTF16(base::WideToUTF16(kTestCases[i].input)),
            misspelling_start, misspelling_length, &suggestions);

    EXPECT_EQ(kTestCases[i].expected_misspelling_start, misspelling_start);
    EXPECT_EQ(kTestCases[i].expected_misspelling_length, misspelling_length);
    if (!kTestCases[i].expected_suggestions) {
      EXPECT_EQ(0UL, suggestions.size());
      continue;
    }

    std::vector<base::string16> expected_suggestions = base::SplitString(
        base::WideToUTF16(kTestCases[i].expected_suggestions),
        base::string16(1, ','), base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    EXPECT_EQ(expected_suggestions.size(), suggestions.size());
    for (size_t j = 0;
         j < std::min(expected_suggestions.size(), suggestions.size()); j++) {
      EXPECT_EQ(expected_suggestions[j], suggestions[j].Utf16());
    }
  }
}
