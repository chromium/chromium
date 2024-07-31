// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/spellcheck/renderer/spellcheck.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/renderer/empty_local_interface_provider.h"
#include "components/spellcheck/renderer/hunspell_engine.h"
#include "components/spellcheck/renderer/spellcheck_language.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"

namespace {

base::FilePath GetHunspellDirectory() {
  base::FilePath hunspell_directory;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                              &hunspell_directory)) {
    return base::FilePath();
  }

  hunspell_directory = hunspell_directory.AppendASCII("third_party");
  hunspell_directory = hunspell_directory.AppendASCII("hunspell_dictionaries");
  return hunspell_directory;
}

}  // namespace

// TODO(groby): This needs to be a BrowserTest for OSX.
class SpellCheckTest : public testing::Test {
 public:
  SpellCheckTest() {
    ReinitializeSpellCheck("en-US");
  }

  void ReinitializeSpellCheck(const std::string& language) {
    UninitializeSpellCheck();
    InitializeSpellCheck(language);
  }

  void UninitializeSpellCheck() {
    spell_check_ = std::make_unique<SpellCheck>(&embedder_provider_);
  }

  bool InitializeIfNeeded() {
    return spell_check()->InitializeIfNeeded();
  }

  void InitializeSpellCheck(const std::string& language) {
    base::FilePath hunspell_directory = GetHunspellDirectory();
    EXPECT_FALSE(hunspell_directory.empty());
    base::FilePath hunspell_file_path =
        spellcheck::GetVersionedFileName(language, hunspell_directory);
    base::File file(hunspell_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(file.IsValid()) << hunspell_file_path << " is not valid"
                                << file.ErrorToString(file.GetLastFileError());
#if BUILDFLAG(IS_APPLE)
    // TODO(groby): Forcing spellcheck to use hunspell, even on OSX.
    // Instead, tests should exercise individual spelling engines.
    spell_check_->languages_.push_back(
        std::make_unique<SpellcheckLanguage>(&embedder_provider_));
    spell_check_->languages_.front()->platform_spelling_engine_ =
        std::make_unique<HunspellEngine>(&embedder_provider_);
    spell_check_->languages_.front()->Init(std::move(file), language);
#else
    spell_check_->AddSpellcheckLanguage(std::move(file), language);
#endif
  }

  ~SpellCheckTest() override {}

  SpellCheck* spell_check() { return spell_check_.get(); }

  bool CheckSpelling(const std::string& word) {
    return spell_check_->languages_.front()
        ->platform_spelling_engine_->CheckSpelling(
            base::ASCIIToUTF16(word), provider_.GetSpellCheckHost());
  }

  bool IsValidContraction(const std::u16string& word) {
    return spell_check_->languages_.front()->IsValidContraction(
        word, provider_.GetSpellCheckHost());
  }

  static void FillSuggestions(
      const std::vector<std::vector<std::u16string>>& suggestions_list,
      std::vector<std::u16string>* optional_suggestions) {
    spellcheck::FillSuggestions(suggestions_list, optional_suggestions);
  }

#if !BUILDFLAG(IS_APPLE)
 protected:
  void TestSpellCheckParagraph(const std::u16string& input,
                               const std::vector<SpellCheckResult>& expected) {
    blink::WebVector<blink::WebTextCheckingResult> results;
    spell_check()->SpellCheckParagraph(input, provider_.GetSpellCheckHost(),
                                       &results);

    EXPECT_EQ(results.size(), expected.size());
    size_t size = std::min(results.size(), expected.size());
    for (size_t j = 0; j < size; ++j) {
      EXPECT_EQ(results[j].decoration, blink::kWebTextDecorationTypeSpelling);
      EXPECT_EQ(results[j].location, expected[j].location);
      EXPECT_EQ(results[j].length, expected[j].length);
    }
  }
#endif

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  spellcheck::EmptyLocalInterfaceProvider embedder_provider_;
  std::unique_ptr<SpellCheck> spell_check_;

 protected:
  TestingSpellCheckProvider provider_{&embedder_provider_};
};

struct MockTextCheckingResult {
  size_t completion_count_ = 0;
  blink::WebVector<blink::WebTextCheckingResult> last_results_;
};

// A fake completion object for verification.
class MockTextCheckingCompletion : public blink::WebTextCheckingCompletion {
 public:
  explicit MockTextCheckingCompletion(MockTextCheckingResult* result)
      : result_(result) {}

  void DidFinishCheckingText(
      const blink::WebVector<blink::WebTextCheckingResult>& results) override {
    result_->completion_count_++;
    result_->last_results_ = results;
  }

  void DidCancelCheckingText() override { result_->completion_count_++; }

  raw_ptr<MockTextCheckingResult> result_;
};

// Operates unit tests for the content::SpellCheck::SpellCheckWord() function
// with the US English dictionary.
// The unit tests in this function consist of:
//   * Tests for the function with empty strings;
//   * Tests for the function with a valid English word;
//   * Tests for the function with a valid non-English word;
//   * Tests for the function with a valid English word with a preceding
//     space character;
//   * Tests for the function with a valid English word with a preceding
//     non-English word;
//   * Tests for the function with a valid English word with a following
//     space character;
//   * Tests for the function with a valid English word with a following
//     non-English word;
//   * Tests for the function with two valid English words concatenated
//     with space characters or non-English words;
//   * Tests for the function with an invalid English word;
//   * Tests for the function with an invalid English word with a preceding
//     space character;
//   * Tests for the function with an invalid English word with a preceding
//     non-English word;
//   * Tests for the function with an invalid English word with a following
//     space character;
//   * Tests for the function with an invalid English word with a following
//     non-English word, and;
//   * Tests for the function with two invalid English words concatenated
//     with space characters or non-English words.
// A test with a "[ROBUSTNESS]" mark shows it is a robustness test and it uses
// grammatically incorrect string.
// TODO(groby): Please feel free to add more tests.
TEST_F(SpellCheckTest, SpellCheckStrings_EN_US) {
  static const struct {
    // A string to be tested.
    const wchar_t* input;
    // An expected result for this test case.
    //   * true: the input string does not have any invalid words.
    //   * false: the input string has one or more invalid words.
    bool expected_result;
    // The position and the length of the first invalid word.
    size_t misspelling_start;
    size_t misspelling_length;
  } kTestCases[] = {
      // Empty strings.
      {L"", true},
      {L" ", true},
      {L"\xA0", true},
      {L"\x3000", true},

      // A valid English word "hello".
      {L"hello", true},
      // A valid Chinese word (meaning "hello") consisting of two CJKV
      // ideographs
      {L"\x4F60\x597D", true},
      // A valid Korean word (meaning "hello") consisting of five hangul
      // syllables
      {L"\xC548\xB155\xD558\xC138\xC694", true},
      // A valid Japanese word (meaning "hello") consisting of five Hiragana
      // letters
      {L"\x3053\x3093\x306B\x3061\x306F", true},
      // A valid Hindi word (meaning ?) consisting of six Devanagari letters
      // (This word is copied from "http://b/issue?id=857583".)
      {L"\x0930\x093E\x091C\x0927\x093E\x0928", true},
      // A valid English word "affix" using a Latin ligature 'ffi'
      {L"a\xFB03x", true},
      // A valid English word "hello" (fullwidth version)
      {L"\xFF28\xFF45\xFF4C\xFF4C\xFF4F", true},
      // Two valid Greek words (meaning "hello") consisting of seven Greek
      // letters
      {L"\x03B3\x03B5\x03B9\x03AC"
       L" "
       L"\x03C3\x03BF\x03C5",
       true},
      // A valid Russian word (meaning "hello") consisting of twelve Cyrillic
      // letters
      {L"\x0437\x0434\x0440\x0430\x0432\x0441"
       L"\x0442\x0432\x0443\x0439\x0442\x0435",
       true},
      // A valid English contraction
      {L"isn't", true},
      // A valid English contraction with a typographical apostrophe.
      {L"isn’t", true},
      // A valid English word enclosed with underscores.
      {L"_hello_", true},

      // A valid English word with a preceding whitespace
      {L" "
       L"hello",
       true},
      // A valid English word with a preceding no-break space
      {L"\xA0"
       L"hello",
       true},
      // A valid English word with a preceding ideographic space
      {L"\x3000"
       L"hello",
       true},
      // A valid English word with a preceding Chinese word
      {L"\x4F60\x597D"
       L"hello",
       true},
      // [ROBUSTNESS] A valid English word with a preceding Korean word
      {L"\xC548\xB155\xD558\xC138\xC694"
       L"hello",
       true},
      // A valid English word with a preceding Japanese word
      {L"\x3053\x3093\x306B\x3061\x306F"
       L"hello",
       true},
      // [ROBUSTNESS] A valid English word with a preceding Hindi word
      {L"\x0930\x093E\x091C\x0927\x093E\x0928"
       L"hello",
       true},
      // [ROBUSTNESS] A valid English word with two preceding Greek words
      {L"\x03B3\x03B5\x03B9\x03AC"
       L" "
       L"\x03C3\x03BF\x03C5"
       L"hello",
       true},
      // [ROBUSTNESS] A valid English word with a preceding Russian word
      {L"\x0437\x0434\x0440\x0430\x0432\x0441"
       L"\x0442\x0432\x0443\x0439\x0442\x0435"
       L"hello",
       true},

      // A valid English word with a following whitespace
      {L"hello"
       L" ",
       true},
      // A valid English word with a following no-break space
      {L"hello"
       L"\xA0",
       true},
      // A valid English word with a following ideographic space
      {L"hello"
       L"\x3000",
       true},
      // A valid English word with a following Chinese word
      {L"hello"
       L"\x4F60\x597D",
       true},
      // [ROBUSTNESS] A valid English word with a following Korean word
      {L"hello"
       L"\xC548\xB155\xD558\xC138\xC694",
       true},
      // A valid English word with a following Japanese word
      {L"hello"
       L"\x3053\x3093\x306B\x3061\x306F",
       true},
      // [ROBUSTNESS] A valid English word with a following Hindi word
      {L"hello"
       L"\x0930\x093E\x091C\x0927\x093E\x0928",
       true},
      // [ROBUSTNESS] A valid English word with two following Greek words
      {L"hello"
       L"\x03B3\x03B5\x03B9\x03AC"
       L" "
       L"\x03C3\x03BF\x03C5",
       true},
      // [ROBUSTNESS] A valid English word with a following Russian word
      {L"hello"
       L"\x0437\x0434\x0440\x0430\x0432\x0441"
       L"\x0442\x0432\x0443\x0439\x0442\x0435",
       true},

      // Two valid English words concatenated with a whitespace
      {L"hello"
       L" "
       L"hello",
       true},
      // Two valid English words concatenated with a no-break space
      {L"hello"
       L"\xA0"
       L"hello",
       true},
      // Two valid English words concatenated with an ideographic space
      {L"hello"
       L"\x3000"
       L"hello",
       true},
      // Two valid English words concatenated with a Chinese word
      {L"hello"
       L"\x4F60\x597D"
       L"hello",
       true},
      // [ROBUSTNESS] Two valid English words concatenated with a Korean word
      {L"hello"
       L"\xC548\xB155\xD558\xC138\xC694"
       L"hello",
       true},
      // Two valid English words concatenated with a Japanese word
      {L"hello"
       L"\x3053\x3093\x306B\x3061\x306F"
       L"hello",
       true},
      // [ROBUSTNESS] Two valid English words concatenated with a Hindi word
      {L"hello"
       L"\x0930\x093E\x091C\x0927\x093E\x0928"
       L"hello",
       true},
      // [ROBUSTNESS] Two valid English words concatenated with two Greek words
      {L"hello"
       L"\x03B3\x03B5\x03B9\x03AC"
       L" "
       L"\x03C3\x03BF\x03C5"
       L"hello",
       true},
      // [ROBUSTNESS] Two valid English words concatenated with a Russian word
      {L"hello"
       L"\x0437\x0434\x0440\x0430\x0432\x0441"
       L"\x0442\x0432\x0443\x0439\x0442\x0435"
       L"hello",
       true},
      // [ROBUSTNESS] Two valid English words concatenated with a contraction
      // character.
      {L"hello:hello", true},

      // An invalid English word
      {L"ifmmp", false, 0, 5},
      // An invalid English word "bffly" containing a Latin ligature 'ffl'
      {L"b\xFB04y", false, 0, 3},
      // An invalid English word "ifmmp" (fullwidth version)
      {L"\xFF29\xFF46\xFF4D\xFF4D\xFF50", false, 0, 5},
      // An invalid English contraction
      {L"jtm'u", false, 0, 5},
      // An invalid English word enclosed with underscores.
      {L"_ifmmp_", false, 1, 5},

      // An invalid English word with a preceding whitespace
      {L" "
       L"ifmmp",
       false, 1, 5},
      // An invalid English word with a preceding no-break space
      {L"\xA0"
       L"ifmmp",
       false, 1, 5},
      // An invalid English word with a preceding ideographic space
      {L"\x3000"
       L"ifmmp",
       false, 1, 5},
      // An invalid English word with a preceding Chinese word
      {L"\x4F60\x597D"
       L"ifmmp",
       false, 2, 5},
      // [ROBUSTNESS] An invalid English word with a preceding Korean word
      {L"\xC548\xB155\xD558\xC138\xC694"
       L"ifmmp",
       false, 5, 5},
      // An invalid English word with a preceding Japanese word
      {L"\x3053\x3093\x306B\x3061\x306F"
       L"ifmmp",
       false, 5, 5},
      // [ROBUSTNESS] An invalid English word with a preceding Hindi word
      {L"\x0930\x093E\x091C\x0927\x093E\x0928"
       L"ifmmp",
       false, 6, 5},
      // [ROBUSTNESS] An invalid English word with two preceding Greek words
      {L"\x03B3\x03B5\x03B9\x03AC"
       L" "
       L"\x03C3\x03BF\x03C5"
       L"ifmmp",
       false, 8, 5},
      // [ROBUSTNESS] An invalid English word with a preceding Russian word
      {L"\x0437\x0434\x0440\x0430\x0432\x0441"
       L"\x0442\x0432\x0443\x0439\x0442\x0435"
       L"ifmmp",
       false, 12, 5},

      // An invalid English word with a following whitespace
      {L"ifmmp"
       L" ",
       false, 0, 5},
      // An invalid English word with a following no-break space
      {L"ifmmp"
       L"\xA0",
       false, 0, 5},
      // An invalid English word with a following ideographic space
      {L"ifmmp"
       L"\x3000",
       false, 0, 5},
      // An invalid English word with a following Chinese word
      {L"ifmmp"
       L"\x4F60\x597D",
       false, 0, 5},
      // [ROBUSTNESS] An invalid English word with a following Korean word
      {L"ifmmp"
       L"\xC548\xB155\xD558\xC138\xC694",
       false, 0, 5},
      // An invalid English word with a following Japanese word
      {L"ifmmp"
       L"\x3053\x3093\x306B\x3061\x306F",
       false, 0, 5},
      // [ROBUSTNESS] An invalid English word with a following Hindi word
      {L"ifmmp"
       L"\x0930\x093E\x091C\x0927\x093E\x0928",
       false, 0, 5},
      // [ROBUSTNESS] An invalid English word with two following Greek words
      {L"ifmmp"
       L"\x03B3\x03B5\x03B9\x03AC"
       L" "
       L"\x03C3\x03BF\x03C5",
       false, 0, 5},
      // [ROBUSTNESS] An invalid English word with a following Russian word
      {L"ifmmp"
       L"\x0437\x0434\x0440\x0430\x0432\x0441"
       L"\x0442\x0432\x0443\x0439\x0442\x0435",
       false, 0, 5},

      // Two invalid English words concatenated with a whitespace
      {L"ifmmp"
       L" "
       L"ifmmp",
       false, 0, 5},
      // Two invalid English words concatenated with a no-break space
      {L"ifmmp"
       L"\xA0"
       L"ifmmp",
       false, 0, 5},
      // Two invalid English words concatenated with an ideographic space
      {L"ifmmp"
       L"\x3000"
       L"ifmmp",
       false, 0, 5},
      // Two invalid English words concatenated with a Chinese word
      {L"ifmmp"
       L"\x4F60\x597D"
       L"ifmmp",
       false, 0, 5},
      // [ROBUSTNESS] Two invalid English words concatenated with a Korean word
      {L"ifmmp"
       L"\xC548\xB155\xD558\xC138\xC694"
       L"ifmmp",
       false, 0, 5},
      // Two invalid English words concatenated with a Japanese word
      {L"ifmmp"
       L"\x3053\x3093\x306B\x3061\x306F"
       L"ifmmp",
       false, 0, 5},
      // [ROBUSTNESS] Two invalid English words concatenated with a Hindi word
      {L"ifmmp"
       L"\x0930\x093E\x091C\x0927\x093E\x0928"
       L"ifmmp",
       false, 0, 5},
      // [ROBUSTNESS] Two invalid English words concatenated with two Greek
      // words
      {L"ifmmp"
       L"\x03B3\x03B5\x03B9\x03AC"
       L" "
       L"\x03C3\x03BF\x03C5"
       L"ifmmp",
       false, 0, 5},
      // [ROBUSTNESS] Two invalid English words concatenated with a Russian word
      {L"ifmmp"
       L"\x0437\x0434\x0440\x0430\x0432\x0441"
       L"\x0442\x0432\x0443\x0439\x0442\x0435"
       L"ifmmp",
       false, 0, 5},
      // [ROBUSTNESS] Two invalid English words concatenated with a contraction
      // character.
      {L"ifmmp:ifmmp", false, 0, 11},

      // [REGRESSION] Issue 13432: "Any word of 13 or 14 characters is not
      // spellcheck" <http://crbug.com/13432>.
      {L"qwertyuiopasd", false, 0, 13},
      {L"qwertyuiopasdf", false, 0, 14},

      // [REGRESSION] Issue 128896: "en_US hunspell dictionary includes
      // acknowledgement but not acknowledgements" <http://crbug.com/128896>
      {L"acknowledgement", true},
      {L"acknowledgements", true},

      // Issue 123290: "Spellchecker should treat numbers as word characters"
      {L"0th", true},
      {L"1st", true},
      {L"2nd", true},
      {L"3rd", true},
      {L"4th", true},
      {L"5th", true},
      {L"6th", true},
      {L"7th", true},
      {L"8th", true},
      {L"9th", true},
      {L"10th", true},
      {L"100th", true},
      {L"1000th", true},
      {L"25", true},
      {L"2012", true},
      {L"100,000,000", true},
      {L"3.141592653", true},
  };
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    size_t misspelling_start;
    size_t misspelling_length;
    bool result = spell_check()->SpellCheckWord(
        base::WideToUTF16(kTestCases[i].input), provider_.GetSpellCheckHost(),
        &misspelling_start, &misspelling_length, nullptr);

    EXPECT_EQ(kTestCases[i].expected_result, result);
    EXPECT_EQ(kTestCases[i].misspelling_start, misspelling_start);
    EXPECT_EQ(kTestCases[i].misspelling_length, misspelling_length);
  }
}

TEST_F(SpellCheckTest, SpellCheckSuggestions_EN_US) {
  static const struct {
    // A string to be tested.
    const wchar_t* input;
    // An expected result for this test case.
    //   * true: the input string does not have any invalid words.
    //   * false: the input string has one or more invalid words.
    bool expected_result;
    // The position and the length of the first invalid word.
    int misspelling_start;
    int misspelling_length;

    // A suggested word that should occur.
    const wchar_t* suggested_word;
  } kTestCases[] = {
    {L"ello", false, 0, 0, L"hello"},
    {L"ello", false, 0, 0, L"cello"},
    {L"wate", false, 0, 0, L"water"},
    {L"wate", false, 0, 0, L"waste"},
    {L"wate", false, 0, 0, L"sate"},
    {L"wate", false, 0, 0, L"ate"},
    {L"jum", false, 0, 0, L"jump"},
    {L"jum", false, 0, 0, L"hum"},
    {L"jum", false, 0, 0, L"sum"},
    {L"jum", false, 0, 0, L"um"},
    {L"alot", false, 0, 0, L"a lot"},
    // A regression test for Issue 36523.
    {L"privliged", false, 0, 0, L"privileged"},
    // TODO (Sidchat): add many more examples.
  };

  for (const auto& test_case : kTestCases) {
    std::vector<std::u16string> suggestions;
    size_t misspelling_start;
    size_t misspelling_length;
    bool result = spell_check()->SpellCheckWord(
        base::WideToUTF16(test_case.input), provider_.GetSpellCheckHost(),
        &misspelling_start, &misspelling_length, &suggestions);

    // Check for spelling.
    EXPECT_EQ(test_case.expected_result, result);

    // Check if the suggested words occur.
    bool suggested_word_is_present = base::Contains(
        suggestions, base::WideToUTF16(test_case.suggested_word));
    EXPECT_TRUE(suggested_word_is_present);
  }
}

// This test verifies our spellchecker can split a text into words and check
// the spelling of each word in the text.
TEST_F(SpellCheckTest, SpellCheckText) {
  static const struct {
    const char* language;
    const wchar_t* input;
  } kTestCases[] = {
      {// Afrikaans
       "af-ZA",
       L"Google se missie is om die w\x00EAreld se inligting te organiseer en "
       L"dit bruikbaar en toeganklik te maak."},
      {// Bulgarian
       "bg-BG",
       L"\x041c\x0438\x0441\x0438\x044f\x0442\x0430 "
       L"\x043d\x0430 Google \x0435 \x0434\x0430 \x043e"
       L"\x0440\x0433\x0430\x043d\x0438\x0437\x0438\x0440"
       L"\x0430 \x0441\x0432\x0435\x0442\x043e\x0432"
       L"\x043d\x0430\x0442\x0430 \x0438\x043d\x0444"
       L"\x043e\x0440\x043c\x0430\x0446\x0438\x044f "
       L"\x0438 \x0434\x0430 \x044f \x043d"
       L"\x0430\x043f\x0440\x0430\x0432\x0438 \x0443"
       L"\x043d\x0438\x0432\x0435\x0440\x0441\x0430\x043b"
       L"\x043d\x043e \x0434\x043e\x0441\x0442\x044a"
       L"\x043f\x043d\x0430 \x0438 \x043f\x043e"
       L"\x043b\x0435\x0437\x043d\x0430."},
      {// Catalan
       "ca-ES",
       L"La missi\x00F3 de Google \x00E9s organitzar la informaci\x00F3 "
       L"del m\x00F3n i fer que sigui \x00FAtil i accessible universalment."},
      {// Czech
       "cs-CZ",
       L"Posl\x00E1n\x00EDm spole\x010Dnosti Google je "
       L"uspo\x0159\x00E1\x0064\x0061t informace z cel\x00E9ho sv\x011Bta "
       L"tak, aby byly v\x0161\x0065obecn\x011B p\x0159\x00EDstupn\x00E9 "
       L"a u\x017Eite\x010Dn\x00E9."},
      {// Welsh
       "cy-GB",
       L"Y genhadaeth yw trefnu gwybodaeth y byd a'i gwneud yn hygyrch ac yn "
       L"ddefnyddiol i bawb."},
      {// Danish
       "da-DK",
       L"Googles "
       L"mission er at organisere verdens information og g\x00F8re den "
       L"almindeligt tilg\x00E6ngelig og nyttig."},
      {// German
       "de-DE",
       L"Das Ziel von Google besteht darin, die auf der Welt vorhandenen "
       L"Informationen zu organisieren und allgemein zug\x00E4nglich und "
       L"nutzbar zu machen."},
      {// Greek
       "el-GR",
       L"\x0391\x03C0\x03BF\x03C3\x03C4\x03BF\x03BB\x03AE "
       L"\x03C4\x03B7\x03C2 Google \x03B5\x03AF\x03BD\x03B1\x03B9 "
       L"\x03BD\x03B1 \x03BF\x03C1\x03B3\x03B1\x03BD\x03CE\x03BD\x03B5\x03B9 "
       L"\x03C4\x03B9\x03C2 "
       L"\x03C0\x03BB\x03B7\x03C1\x03BF\x03C6\x03BF\x03C1\x03AF\x03B5\x03C2 "
       L"\x03C4\x03BF\x03C5 \x03BA\x03CC\x03C3\x03BC\x03BF\x03C5 "
       L"\x03BA\x03B1\x03B9 \x03BD\x03B1 \x03C4\x03B9\x03C2 "
       L"\x03BA\x03B1\x03B8\x03B9\x03C3\x03C4\x03AC "
       L"\x03C0\x03C1\x03BF\x03C3\x03B2\x03AC\x03C3\x03B9\x03BC\x03B5\x03C2 "
       L"\x03BA\x03B1\x03B9 \x03C7\x03C1\x03AE\x03C3\x03B9\x03BC\x03B5\x03C2."},
      {// English (Australia)
       "en-AU",
       L"Google's mission is to organise the world's information and make it "
       L"universally accessible and useful."},
      {// English (Canada)
       "en-CA",
       L"Google's mission is to organize the world's information and make it "
       L"universally accessible and useful."},
      {// English (United Kingdom)
       "en-GB",
       L"Google's mission is to organise the world's information and make it "
       L"universally accessible and useful."},
      {// English (United States)
       "en-US",
       L"Google's mission is to organize the world's information and make it "
       L"universally accessible and useful."},
      {// Spanish
       "es-ES",
       L"La misi\x00F3n de "
       // L"Google" - to be added.
       L" es organizar la informaci\x00F3n mundial "
       L"para que resulte universalmente accesible y \x00FAtil."},
      {
          // Estonian
          "et-EE",
          // L"Google'ile " - to be added.
          L"\x00FClesanne on korraldada maailma teavet ja teeb selle "
          L"k\x00F5igile k\x00E4ttesaadavaks ja kasulikuks.",
      },
      {// Persian
       "fa",
       L"\x0686\x0647 \x0637\x0648\x0631 \x0622\x06cc\x0627 \x0634\x0645\x0627 "
       L"\x0627\x06cc\x0631\x0627\x0646\x06cc \x0647\x0633\x062a\x06cc\x062f"},
      {// Faroese
       "fo-FO",
       L"Google er at samskipa alla vitan \x00ED heiminum og gera hana alment "
       L"atkomiliga og n\x00FDtiliga."},
      {// French
       "fr-FR",
       L"Google a pour mission d'organiser les informations \x00E0 "
       L"l'\x00E9\x0063helle mondiale dans le but de les rendre accessibles "
       L"et utiles \x00E0 tous."},
      {// Hebrew
       "he-IL",
       L"\x05D4\x05DE\x05E9\x05D9\x05DE\x05D4 \x05E9\x05DC Google "
       L"\x05D4\x05D9\x05D0 \x05DC\x05D0\x05E8\x05D2\x05DF "
       L"\x05D0\x05EA \x05D4\x05DE\x05D9\x05D3\x05E2 "
       L"\x05D4\x05E2\x05D5\x05DC\x05DE\x05D9 "
       L"\x05D5\x05DC\x05D4\x05E4\x05D5\x05DA \x05D0\x05D5\x05EA\x05D5 "
       L"\x05DC\x05D6\x05DE\x05D9\x05DF "
       L"\x05D5\x05E9\x05D9\x05DE\x05D5\x05E9\x05D9 \x05D1\x05DB\x05DC "
       L"\x05D4\x05E2\x05D5\x05DC\x05DD. "
       // Two words with ASCII double/single quoation marks.
       L"\x05DE\x05E0\x05DB\x0022\x05DC \x05E6\x0027\x05D9\x05E4\x05E1"},
      {// Hindi
       "hi-IN",
       L"Google \x0915\x093E \x092E\x093F\x0936\x0928 "
       L"\x0926\x0941\x0928\x093F\x092F\x093E \x0915\x0940 "
       L"\x091C\x093E\x0928\x0915\x093E\x0930\x0940 \x0915\x094B "
       L"\x0935\x094D\x092F\x0935\x0938\x094D\x0925\x093F\x0924 "
       L"\x0915\x0930\x0928\x093E \x0914\x0930 \x0909\x0938\x0947 "
       L"\x0938\x093E\x0930\x094D\x0935\x092D\x094C\x092E\x093F\x0915 "
       L"\x0930\x0942\x092A \x0938\x0947 \x092A\x0939\x0941\x0901\x091A "
       L"\x092E\x0947\x0902 \x0914\x0930 \x0909\x092A\x092F\x094B\x0917\x0940 "
       L"\x092C\x0928\x093E\x0928\x093E \x0939\x0948."},
      {
#if !BUILDFLAG(IS_WIN)
          // Hungarian
          "hu-HU",
          L"A Google azt a k\x00FCldet\x00E9st v\x00E1llalta mag\x00E1ra, "
          L"hogy a vil\x00E1gon fellelhet\x0151 inform\x00E1\x0063i\x00F3kat "
          L"rendszerezze \x00E9s \x00E1ltal\x00E1nosan "
          L"el\x00E9rhet\x0151v\x00E9, "
          L"illetve haszn\x00E1lhat\x00F3v\x00E1 tegye."},
      {
#endif  // !BUILDFLAG(IS_WIN)
        // Croatian
          "hr-HR",
          // L"Googleova " - to be added.
          L"je misija organizirati svjetske informacije i u\x010Diniti ih "
          // L"univerzalno " - to be added.
          L"pristupa\x010Dnima i korisnima."},
      {// Armenian
       "hy",
       L"Google- \x056B \x0561\x057C\x0561\x0584\x0565\x056C\x0578\x0582\x0569"
       L"\x0575\x0578\x0582\x0576\x0576 \x0567 \x0570\x0561\x0574\x0561\x0577"
       L"\x056D\x0561\x0580\x0570\x0561\x0575\x056B\x0576 \x057F\x0565\x0572"
       L"\x0565\x056F\x0561\x057F\x057E\x0578\x0582\x0569\x0575\x0578\x0582"
       L"\x0576\x0568 \x056F\x0561\x0566\x0574\x0561\x056F\x0565\x0580\x057A"
       L"\x0565\x056C \x0565\x0582 \x0564\x0561\x0580\x0571\x0576\x0565\x056C "
       L"\x0561\x0575\x0576 \x0570\x0561\x0574\x0568\x0576\x0564\x0570\x0561"
       L"\x0576\x0578\x0582\x0580 \x0570\x0561\x057D\x0561\x0576\x0565\x056C"
       L"\x056B \x0565\x0582 \x0585\x0563\x057F\x0561\x056F\x0561\x0580:"},
      {// Indonesian
       "id-ID",
       L"Misi Google adalah untuk mengelola informasi dunia dan membuatnya "
       L"dapat diakses dan bermanfaat secara universal."},
      {// Italian
       "it-IT",
       L"La missione di Google \x00E8 organizzare le informazioni a livello "
       L"mondiale e renderle universalmente accessibili e fruibili."},
      {// Lithuanian
       "lt-LT",
       L"\x201EGoogle\x201C tikslas \x2013 rinkti ir sisteminti pasaulio "
       L"informacij\x0105 bei padaryti j\x0105 prieinam\x0105 ir "
       L"nauding\x0105 visiems."},
      {// Latvian
       "lv-LV",
       L"Google uzdevums ir k\x0101rtot pasaules inform\x0101"
       L"ciju un padar\x012Bt to univers\x0101li pieejamu un noder\x012Bgu."},
      {// Norwegian
       "nb-NO",
       // L"Googles " - to be added.
       L"m\x00E5l er \x00E5 organisere informasjonen i verden og "
       L"gj\x00F8re den tilgjengelig og nyttig for alle."},
      {// Dutch
       "nl-NL",
       L"Het doel van Google is om alle informatie wereldwijd toegankelijk "
       L"en bruikbaar te maken."},
      {// Polish
       "pl-PL",
       L"Misj\x0105 Google jest uporz\x0105"
       L"dkowanie \x015Bwiatowych "
       L"zasob\x00F3w informacji, aby sta\x0142y si\x0119 one powszechnie "
       L"dost\x0119pne i u\x017Cyteczne."},
      {
#if !BUILDFLAG(IS_WIN)
          // Portuguese (Brazil)
          "pt-BR",
          L"A miss\x00E3o do "
#if !BUILDFLAG(IS_APPLE)
          L"Google "
#endif
          L"\x00E9 organizar as informa\x00E7\x00F5"
          L"es do mundo todo e "
#if !BUILDFLAG(IS_APPLE)
          L"torn\x00E1-las "
#endif
          L"acess\x00EDveis e \x00FAteis em car\x00E1ter universal."},
      {
#endif  // !BUILDFLAG(IS_WIN)
        // Portuguese (Portugal)
          "pt-PT",
          L"O "
#if !BUILDFLAG(IS_APPLE)
          L"Google "
#endif
          L"tem por miss\x00E3o organizar a informa\x00E7\x00E3o do "
          L"mundo e "
#if !BUILDFLAG(IS_APPLE)
          L"torn\x00E1-la "
#endif
          L"universalmente acess\x00EDvel e \x00FAtil"},
      {// Romanian
       "ro-RO",
       L"Misiunea Google este de a organiza informa\x021B3iile lumii \x0219i "
       L"de "
       L"a le face accesibile \x0219i utile la nivel universal."},
      {// Russian
       "ru-RU",
       L"\x041C\x0438\x0441\x0441\x0438\x044F Google "
       L"\x0441\x043E\x0441\x0442\x043E\x0438\x0442 \x0432 "
       L"\x043E\x0440\x0433\x0430\x043D\x0438\x0437\x0430\x0446\x0438\x0438 "
       L"\x043C\x0438\x0440\x043E\x0432\x043E\x0439 "
       L"\x0438\x043D\x0444\x043E\x0440\x043C\x0430\x0446\x0438\x0438, "
       L"\x043E\x0431\x0435\x0441\x043F\x0435\x0447\x0435\x043D\x0438\x0438 "
       L"\x0435\x0435 "
       L"\x0434\x043E\x0441\x0442\x0443\x043F\x043D\x043E\x0441\x0442\x0438 "
       L"\x0438 \x043F\x043E\x043B\x044C\x0437\x044B \x0434\x043B\x044F "
       L"\x0432\x0441\x0435\x0445."
       // A Russian word including U+0451. (Bug 15558 <http://crbug.com/15558>)
       L"\x0451\x043B\x043A\x0430"},
      {// Serbo-Croatian (Serbian Latin)
       "sh",
       L"Guglova misija je organizirati svjetske informacije i u\x010diniti ih "
       L"univerzalno dostupnim i korisnim."},
      {// Serbian
       "sr",
       L"\x0413\x0443\x0433\x043B\x043E\x0432\x0430 "
       L"\x043C\x0438\x0441\x0438\x0458\x0430 \x0458\x0435 \x0434\x0430 "
       L"\x043E\x0440\x0433\x0430\x043D\x0438\x0437\x0443\x0458\x0435 "
       L"\x0441\x0432\x0435\x0442\x0441\x043A\x0435 "
       L"\x0438\x043D\x0444\x043E\x0440\x043C\x0430\x0446\x0438\x0458\x0435 "
       L"\x0438 \x0443\x0447\x0438\x043D\x0438 \x0438\x0445 "
       L"\x0443\x043D\x0438\x0432\x0435\x0440\x0437\x0430\x043B\x043D\x0438"
       L"\x043C \x0434\x043E\x0441\x0442\x0443\x043F\x043D\x0438\x043C \x0438 "
       L"\x043A\x043E\x0440\x0438\x0441\x043D\x0438\x043C."},
      {// Slovak
       "sk-SK",
       L"Spolo\x010Dnos\x0165 Google si dala za \x00FAlohu usporiada\x0165 "
       L"inform\x00E1\x0063ie "
       L"z cel\x00E9ho sveta a zabezpe\x010Di\x0165, "
       L"aby boli v\x0161eobecne dostupn\x00E9 a u\x017Eito\x010Dn\x00E9."},
      {// Slovenian
       "sl-SI",
       // L"Googlovo " - to be added.
       L"poslanstvo je organizirati svetovne informacije in "
       L"omogo\x010Diti njihovo dostopnost in s tem uporabnost za vse."},
      {// Swedish
       "sv-SE",
       L"Googles m\x00E5ls\x00E4ttning \x00E4r att ordna v\x00E4rldens "
       L"samlade information och g\x00F6ra den tillg\x00E4nglig f\x00F6r "
       L"alla."},
      {
#if !BUILDFLAG(IS_WIN)
          // Turkish
          "tr-TR",
          // L"Google\x2019\x0131n " - to be added.
          L"misyonu, d\x00FCnyadaki t\x00FCm bilgileri "
          L"organize etmek ve evrensel olarak eri\x015Filebilir ve "
          L"kullan\x0131\x015Fl\x0131 k\x0131lmakt\x0131r."},
      {
#endif  // !BUILDFLAG(IS_WIN)
        // Ukrainian
          "uk-UA",
          L"\x041c\x0456\x0441\x0456\x044f "
          L"\x043a\x043e\x043c\x043f\x0430\x043d\x0456\x0457 Google "
          L"\x043f\x043e\x043b\x044f\x0433\x0430\x0454 \x0432 "
          L"\x0442\x043e\x043c\x0443, \x0449\x043e\x0431 "
          L"\x0443\x043f\x043e\x0440\x044f\x0434\x043a\x0443\x0432\x0430\x0442"
          L"\x0438 "
          L"\x0456\x043d\x0444\x043e\x0440\x043c\x0430\x0446\x0456\x044e "
          L"\x0437 \x0443\x0441\x044c\x043e\x0433\x043e "
          L"\x0441\x0432\x0456\x0442\x0443 \x0442\x0430 "
          L"\x0437\x0440\x043e\x0431\x0438\x0442\x0438 \x0457\x0457 "
          L"\x0443\x043d\x0456\x0432\x0435\x0440\x0441\x0430\x043b\x044c\x043d"
          L"\x043e \x0434\x043e\x0441\x0442\x0443\x043f\x043d\x043e\x044e "
          L"\x0442\x0430 \x043a\x043e\x0440\x0438\x0441\x043d\x043e\x044e."},
      {// Vietnamese
       "vi-VN",
       L"Nhi\x1EC7m v\x1EE5 c\x1EE7\x0061 "
       L"Google la \x0111\x1EC3 t\x1ED5 ch\x1EE9\x0063 "
       L"c\x00E1\x0063 th\x00F4ng tin c\x1EE7\x0061 "
       L"th\x1EBF gi\x1EDBi va l\x00E0m cho n\x00F3 universal c\x00F3 "
       L"th\x1EC3 truy c\x1EADp va h\x1EEFu d\x1EE5ng h\x01A1n."},
      {
#if !BUILDFLAG(IS_WIN)
          // Korean
          "ko",
          L"Google\xC758 \xBAA9\xD45C\xB294 \xC804\xC138\xACC4\xC758 "
          L"\xC815\xBCF4\xB97C \xCCB4\xACC4\xD654\xD558\xC5EC "
          L"\xBAA8\xB450\xAC00 "
          L"\xD3B8\xB9AC\xD558\xAC8C \xC774\xC6A9\xD560 \xC218 "
          L"\xC788\xB3C4\xB85D \xD558\xB294 \xAC83\xC785\xB2C8\xB2E4."},
      {
#endif  // !BUILDFLAG(IS_WIN)
        // Albanian
          "sq",
          L"Misioni i Google \x00EBsht\x00EB q\x00EB t\x00EB organizoj\x00EB "
          L"informacionin e bot\x00EBs dhe t\x00EB b\x00EBjn\x00EB at\x00EB "
          L"universalisht t\x00EB arritshme dhe t\x00EB dobishme."},
      {// Tamil
       "ta",
       L"Google \x0B87\x0BA9\x0BCD "
       L"\x0BA8\x0BC7\x0BBE\x0B95\x0BCD\x0B95\x0BAE\x0BCD "
       L"\x0B89\x0BB2\x0B95\x0BBF\x0BA9\x0BCD \x0BA4\x0B95\x0BB5\x0BB2\x0BCD "
       L"\x0B8F\x0BB1\x0BCD\x0BAA\x0BBE\x0B9F\x0BC1 \x0B87\x0BA4\x0BC1 "
       L"\x0B89\x0BB2\x0B95\x0BB3\x0BBE\x0BB5\x0BBF\x0BAF "
       L"\x0B85\x0BA3\x0BC1\x0B95\x0B95\x0BCD \x0B95\x0BC2\x0B9F\x0BBF\x0BAF "
       L"\x0BAE\x0BB1\x0BCD\x0BB1\x0BC1\x0BAE\x0BCD "
       L"\x0BAA\x0BAF\x0BA9\x0BC1\x0BB3\x0BCD\x0BB3 "
       L"\x0B9A\x0BC6\x0BAF\x0BCD\x0BAF \x0B89\x0BB3\x0BCD\x0BB3\x0BA4\x0BC1."},
      {// Tajik
       "tg",
       L"\x041c\x0438\x0441\x0441\x0438\x044f\x0438 Google \x0438\x043d "
       L"\x043c\x0443\x0440\x0430\x0442\x0442\x0430\x0431 "
       L"\x0441\x043e\x0445\x0442\x0430\x043d\x0438 "
       L"\x043c\x0430\x044a\x043b\x0443\x043c\x043e\x0442\x04b3\x043e\x0438 "
       L"\x043c\x0430\x0432\x04b7\x0443\x0434\x0430, \x043e\x0441\x043e\x043d "
       L"\x043d\x0430\x043c\x0443\x0434\x0430\x043d\x0438 "
       L"\x0438\x0441\x0442\x0438\x0444\x043e\x0434\x0430\x0431\x0430\x0440"
       L"\x04e3 \x0432\x0430 \x0434\x0430\x0441\x0442\x0440\x0430\x0441\x0438 "
       L"\x0443\x043c\x0443\x043c "
       L"\x0433\x0430\x0440\x0434\x043e\x043d\x0438\x0434\x0430\x043d\x0438 "
       L"\x043e\x043d\x04b3\x043e \x0430\x0441\x0442."},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    ReinitializeSpellCheck(kTestCases[i].language);
    size_t misspelling_start = 0;
    size_t misspelling_length = 0;
    bool result = spell_check()->SpellCheckWord(
        base::WideToUTF16(kTestCases[i].input), provider_.GetSpellCheckHost(),
        &misspelling_start, &misspelling_length, nullptr);

    EXPECT_TRUE(result)
        << "\""
        << std::wstring(kTestCases[i].input).substr(
               misspelling_start, misspelling_length)
        << "\" is misspelled in "
        << kTestCases[i].language
        << ".";
    EXPECT_EQ(0u, misspelling_start);
    EXPECT_EQ(0u, misspelling_length);
  }
}

// Verify that our SpellCheck::SpellCheckWord() returns false when it checks
// misspelled words.
TEST_F(SpellCheckTest, MisspelledWords) {
  static const struct {
    const char* language;
    const wchar_t* input;
  } kTestCases[] = {
    {
      // A misspelled word for English
      "en-US",
      L"aaaaaaaaaa",
    }, {
      // A misspelled word for Greek.
      "el-GR",
      L"\x03B1\x03B1\x03B1\x03B1\x03B1\x03B1\x03B1\x03B1\x03B1\x03B1",
    }, {
      // A misspelled word for Persian.
      "fa",
      L"\x06cc\x06a9\x06cc\x0634\x0627\x0646",
    }, {
      // A misspelled word for Hebrew
      "he-IL",
      L"\x05D0\x05D0\x05D0\x05D0\x05D0\x05D0\x05D0\x05D0\x05D0\x05D0",
    }, {
      // Hindi
      "hi-IN",
      L"\x0905\x0905\x0905\x0905\x0905\x0905\x0905\x0905\x0905\x0905",
    }, {
      // A misspelled word for Russian
      "ru-RU",
      L"\x0430\x0430\x0430\x0430\x0430\x0430\x0430\x0430\x0430\x0430",
    },
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    ReinitializeSpellCheck(kTestCases[i].language);

    std::u16string word(base::WideToUTF16(kTestCases[i].input));
    size_t word_length = word.length();
    size_t misspelling_start = 0;
    size_t misspelling_length = 0;
    bool result = spell_check()->SpellCheckWord(
        word, provider_.GetSpellCheckHost(), &misspelling_start,
        &misspelling_length, nullptr);
    EXPECT_FALSE(result);
    EXPECT_EQ(0u, misspelling_start);
    EXPECT_EQ(word_length, misspelling_length);
  }
}

// Since SpellCheck::SpellCheckParagraph is not implemented on Mac,
// we skip these SpellCheckParagraph tests on Mac.
#if !BUILDFLAG(IS_APPLE)

// Make sure SpellCheckParagraph does not crash if the input is empty.
TEST_F(SpellCheckTest, SpellCheckParagraphEmptyParagraph) {
  std::vector<SpellCheckResult> expected;
  TestSpellCheckParagraph(u"", expected);
}

// A simple test case having no misspellings.
TEST_F(SpellCheckTest, SpellCheckParagraphNoMisspellings) {
  const std::u16string text = u"apple";
  std::vector<SpellCheckResult> expected;
  TestSpellCheckParagraph(text, expected);
}

// A simple test case having one misspelling.
TEST_F(SpellCheckTest, SpellCheckParagraphSingleMisspellings) {
  const std::u16string text = u"zz";
  std::vector<SpellCheckResult> expected;
  expected.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 2));

  TestSpellCheckParagraph(text, expected);
}

// A simple test case having multiple misspellings.
TEST_F(SpellCheckTest, SpellCheckParagraphMultipleMisspellings) {
  const std::u16string text = u"zz, zz";
  std::vector<SpellCheckResult> expected;
  expected.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 2));
  expected.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 4, 2));

  TestSpellCheckParagraph(text, expected);
}

// Make sure a relatively long (correct) sentence can be spellchecked.
TEST_F(SpellCheckTest, SpellCheckParagraphLongSentence) {
  std::vector<SpellCheckResult> expected;
  // The text is taken from US constitution preamble.
  const std::u16string text =
      u"We the people of the United States, in order to form a more perfect "
      u"union, establish justice, insure domestic tranquility, provide for "
      u"the common defense, promote the general welfare, and secure the "
      u"blessings of liberty to ourselves and our posterity, do ordain and "
      u"establish this Constitution for the United States of America.";

  TestSpellCheckParagraph(text, expected);
}

// Make sure all misspellings can be found in a relatively long sentence.
TEST_F(SpellCheckTest, SpellCheckParagraphLongSentenceMultipleMisspellings) {
  std::vector<SpellCheckResult> expected;

  // All 'the' are converted to 'hte' in US consitition preamble.
  const std::u16string text =
      u"We hte people of hte United States, in order to form a more perfect "
      u"union, establish justice, insure domestic tranquility, provide for "
      u"hte common defense, promote hte general welfare, and secure hte "
      u"blessings of liberty to ourselves and our posterity, do ordain and "
      u"establish this Constitution for hte United States of America.";

  expected.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 3, 3));
  expected.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 17, 3));
  expected.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 135, 3));
  expected.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 163, 3));
  expected.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 195, 3));
  expected.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 298, 3));

  TestSpellCheckParagraph(text, expected);
}

// We also skip RequestSpellCheck tests on Mac, because a system spellchecker
// is used on Mac instead of SpellCheck::RequestTextChecking.

// Make sure RequestTextChecking does not crash if input is empty.
TEST_F(SpellCheckTest, RequestSpellCheckWithEmptyString) {
  MockTextCheckingResult completion;

  spell_check()->RequestTextChecking(
      std::u16string(),
      std::make_unique<MockTextCheckingCompletion>(&completion),
      provider_.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(completion.completion_count_, 1U);
}

// A simple test case having no misspellings.
TEST_F(SpellCheckTest, RequestSpellCheckWithoutMisspelling) {
  MockTextCheckingResult completion;

  const std::u16string text = u"hello";
  spell_check()->RequestTextChecking(
      text, std::make_unique<MockTextCheckingCompletion>(&completion),
      provider_.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(completion.completion_count_, 1U);
}

// A simple test case having one misspelling.
TEST_F(SpellCheckTest, RequestSpellCheckWithSingleMisspelling) {
  MockTextCheckingResult completion;

  const std::u16string text = u"apple, zz";
  spell_check()->RequestTextChecking(
      text, std::make_unique<MockTextCheckingCompletion>(&completion),
      provider_.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(completion.completion_count_, 1U);
  ASSERT_EQ(completion.last_results_.size(), 1U);
  EXPECT_EQ(completion.last_results_[0].location, 7);
  EXPECT_EQ(completion.last_results_[0].length, 2);
}

// A simple test case having a few misspellings.
TEST_F(SpellCheckTest, RequestSpellCheckWithMisspellings) {
  MockTextCheckingResult completion;

  const std::u16string text = u"apple, zz, orange, zz";
  spell_check()->RequestTextChecking(
      text, std::make_unique<MockTextCheckingCompletion>(&completion),
      provider_.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(completion.completion_count_, 1U);
  ASSERT_EQ(completion.last_results_.size(), 2U);
  EXPECT_EQ(completion.last_results_[0].location, 7);
  EXPECT_EQ(completion.last_results_[0].length, 2);
  EXPECT_EQ(completion.last_results_[1].location, 19);
  EXPECT_EQ(completion.last_results_[1].length, 2);
}

// A test case that multiple requests comes at once. Make sure all
// requests are processed.
TEST_F(SpellCheckTest, RequestSpellCheckWithMultipleRequests) {
  MockTextCheckingResult completion[3];

  const std::u16string text[3] = {u"what, zz", u"apple, zz", u"orange, zz"};

  for (int i = 0; i < 3; ++i)
    spell_check()->RequestTextChecking(
        text[i], std::make_unique<MockTextCheckingCompletion>(&completion[i]),
        provider_.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(completion[i].completion_count_, 1U);
    ASSERT_EQ(completion[i].last_results_.size(), 1U);
    EXPECT_EQ(completion[i].last_results_[0].location, 6 + i);
    EXPECT_EQ(completion[i].last_results_[0].length, 2);
  }
}

// A test case that spellchecking is requested before initializing.
// In this case, we postpone to post a request.
TEST_F(SpellCheckTest, RequestSpellCheckWithoutInitialization) {
  UninitializeSpellCheck();

  MockTextCheckingResult completion;
  const std::u16string text = u"zz";

  spell_check()->RequestTextChecking(
      text, std::make_unique<MockTextCheckingCompletion>(&completion),
      provider_.GetWeakPtr());

  // The task will not be posted yet.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(completion.completion_count_, 0U);
}

// Requests several spellchecking before initializing. Except the last one,
// posting requests is cancelled and text is rendered as correct one.
TEST_F(SpellCheckTest, RequestSpellCheckMultipleTimesWithoutInitialization) {
  UninitializeSpellCheck();

  MockTextCheckingResult completion[3];
  const std::u16string text[3] = {u"what, zz", u"apple, zz", u"orange, zz"};

  // Calls RequestTextchecking a few times.
  for (int i = 0; i < 3; ++i)
    spell_check()->RequestTextChecking(
        text[i], std::make_unique<MockTextCheckingCompletion>(&completion[i]),
        provider_.GetWeakPtr());

  // The last task will be posted after initialization, however the other
  // requests should be pressed without spellchecking.
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < 2; ++i)
    EXPECT_EQ(completion[i].completion_count_, 1U);
  EXPECT_EQ(completion[2].completion_count_, 0U);

  // Checks the last request is processed after initialization.
  InitializeSpellCheck("en-US");

  // Calls PostDelayedSpellCheckTask instead of OnInit here for simplicity.
  spell_check()->PostDelayedSpellCheckTask(
      spell_check()->pending_request_param_.release());
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(completion[i].completion_count_, 1U);
}

#endif

// Verify that the SpellCheck class keeps the spelling marker added to a
// misspelled word "zz".
TEST_F(SpellCheckTest, CreateTextCheckingResultsKeepsMarkers) {
  std::u16string text = u"zz";
  std::vector<SpellCheckResult> spellcheck_results;
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 0, 2, std::u16string()));
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spell_check()->CreateTextCheckingResults(
      SpellCheck::USE_HUNSPELL_FOR_GRAMMAR, provider_.GetSpellCheckHost(), 0,
      text, spellcheck_results, &textcheck_results);
  ASSERT_EQ(spellcheck_results.size(), textcheck_results.size());
  EXPECT_EQ(blink::kWebTextDecorationTypeSpelling,
            textcheck_results[0].decoration);
  EXPECT_EQ(spellcheck_results[0].location, textcheck_results[0].location);
  EXPECT_EQ(spellcheck_results[0].length, textcheck_results[0].length);
}

// Verify that the SpellCheck class replaces the spelling marker added to a
// contextually-misspelled word "bean" with a grammar marker.
TEST_F(SpellCheckTest, CreateTextCheckingResultsAddsGrammarMarkers) {
  std::u16string text = u"I have bean to USA.";
  std::vector<SpellCheckResult> spellcheck_results;
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 7, 4, std::u16string()));
  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spell_check()->CreateTextCheckingResults(
      SpellCheck::USE_HUNSPELL_FOR_GRAMMAR, provider_.GetSpellCheckHost(), 0,
      text, spellcheck_results, &textcheck_results);
  ASSERT_EQ(spellcheck_results.size(), textcheck_results.size());
  EXPECT_EQ(blink::kWebTextDecorationTypeGrammar,
            textcheck_results[0].decoration);
  EXPECT_EQ(spellcheck_results[0].location, textcheck_results[0].location);
  EXPECT_EQ(spellcheck_results[0].length, textcheck_results[0].length);
}

// Verify that the SpellCheck preserves the original apostrophe type in the
// checked text, regardless of the type of apostrophe the browser returns.
TEST_F(SpellCheckTest, CreateTextCheckingResultsKeepsTypographicalApostrophe) {
  std::u16string text = u"Ik've havn’t ni'n’out-s I've I’ve";
  std::vector<SpellCheckResult> spellcheck_results;

  // All typewriter apostrophe results.
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 0, 5, u"I've"));
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 6, 6, u"haven't"));
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 13, 10, u"in'n'out's"));

  // Replacements that differ only by apostrophe type should be ignored.
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 24, 4, u"I've"));
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 29, 4, u"I've"));

  // All typographical apostrophe results.
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 0, 5, u"I’ve"));
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 6, 6, u"haven’t"));
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 13, 10, u"in’n’out’s"));

  // Replacements that differ only by apostrophe type should be ignored.
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 24, 4, u"I’ve"));
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 29, 4, u"I’ve"));

  // If we have no suggested replacements, we should keep this misspelling.
  spellcheck_results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 5, std::vector<std::u16string>()));

  // If we have multiple replacements that all differ only by apostrophe type,
  // we should ignore this misspelling.
  spellcheck_results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 11,
      std::vector<std::u16string>({u"Ik've havn'", u"Ik’ve havn’"})));

  // If we have multiple replacements where some only differ by apostrophe type
  // and some don't, we should keep this misspelling, but remove the
  // replacements that only differ by apostrophe type.
  spellcheck_results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 5,
      std::vector<std::u16string>({u"I've", u"Ive", u"Ik’ve"})));

  // Similar to the previous case except with the apostrophe changing from
  // typographical to straight instead of the other direction
  spellcheck_results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 6, 6,
      std::vector<std::u16string>({u"havn't", u"havnt", u"haven't"})));

  // If we have multiple replacements, none of which differ only by apostrophe
  // type, we should keep this misspelling.
  spellcheck_results.push_back(
      SpellCheckResult(SpellCheckResult::SPELLING, 6, 6,
                       std::vector<std::u16string>({u"have", u"haven't"})));

  blink::WebVector<blink::WebTextCheckingResult> textcheck_results;
  spell_check()->CreateTextCheckingResults(
      SpellCheck::USE_HUNSPELL_FOR_GRAMMAR, provider_.GetSpellCheckHost(), 0,
      text, spellcheck_results, &textcheck_results);

  static std::vector<std::vector<const wchar_t*>> kExpectedReplacements = {
      {L"I've"},
      {L"haven’t"},
      {L"in'n’out's"},
      {L"I've"},
      {L"haven’t"},
      {L"in'n’out’s"},
      std::vector<const wchar_t*>(),
      {L"I've", L"Ive"},
      {L"havnt", L"haven’t"},
      {L"have", L"haven’t"},
  };

  ASSERT_EQ(kExpectedReplacements.size(), textcheck_results.size());
  for (size_t i = 0; i < kExpectedReplacements.size(); ++i) {
    ASSERT_EQ(kExpectedReplacements[i].size(),
              textcheck_results[i].replacements.size());
    for (size_t j = 0; j < kExpectedReplacements[i].size(); ++j) {
      EXPECT_EQ(base::WideToUTF16(kExpectedReplacements[i][j]),
                textcheck_results[i].replacements[j].Utf16())
          << "i=" << i << "\nj=" << j << "\nactual: \""
          << textcheck_results[i].replacements[j].Utf16() << "\"";
    }
  }
}

// Checks some words that should be present in all English dictionaries.
TEST_F(SpellCheckTest, EnglishWords) {
  static const struct {
    const char* input;
    bool should_pass;
  } kTestCases[] = {
    // Issue 146093: "Chromebook" and "Chromebox" not included in spell-checking
    // dictionary.
    {"Chromebook", true},
    {"Chromebooks", true},
    {"Chromebox", true},
    {"Chromeboxes", true},
    {"Chromeblade", true},
    {"Chromeblades", true},
    {"Chromebase", true},
    {"Chromebases", true},
    // Issue 94708: Spell-checker incorrectly reports whisky as misspelled.
    {"whisky", true},
    {"whiskey", true},
    {"whiskies", true},
    // Issue 98678: "Recency" should be included in client-side dictionary.
    {"recency", true},
    {"recencies", false},
    // Issue 140486
    {"movie", true},
    {"movies", true},
  };

  static const char* const kLocales[] = { "en-GB", "en-US", "en-CA", "en-AU" };

  for (size_t j = 0; j < std::size(kLocales); ++j) {
    ReinitializeSpellCheck(kLocales[j]);
    for (size_t i = 0; i < std::size(kTestCases); ++i) {
      size_t misspelling_start = 0;
      size_t misspelling_length = 0;
      bool result = spell_check()->SpellCheckWord(
          base::ASCIIToUTF16(kTestCases[i].input),
          provider_.GetSpellCheckHost(), &misspelling_start,
          &misspelling_length, nullptr);

      EXPECT_EQ(kTestCases[i].should_pass, result) << kTestCases[i].input <<
          " in " << kLocales[j];
    }
  }
}

// Checks that NOSUGGEST works in English dictionaries.
TEST_F(SpellCheckTest, NoSuggest) {
  ReinitializeSpellCheck("xx-XX");

  static const struct {
    const char* input;
    const char* suggestion;
    bool should_pass;
  } kTestCases[] = {{"typograpy", "typographit", true},
                    {"typograpy", "typographits", true}};

  for (const auto& test_case : kTestCases) {
    // First check that the NOSUGGEST flag didn't mark this word as not being in
    // the dictionary.
    size_t misspelling_start = 0;
    size_t misspelling_length = 0;
    bool result = spell_check()->SpellCheckWord(
        base::ASCIIToUTF16(test_case.suggestion), provider_.GetSpellCheckHost(),
        &misspelling_start, &misspelling_length, nullptr);

    EXPECT_EQ(test_case.should_pass, result) << test_case.suggestion;

    // Now verify that this test case does not show up as a suggestion.
    std::vector<std::u16string> suggestions;
    result = spell_check()->SpellCheckWord(
        base::ASCIIToUTF16(test_case.input), provider_.GetSpellCheckHost(),
        &misspelling_start, &misspelling_length, &suggestions);

    // Input word should be a misspelling.
    EXPECT_FALSE(result) << test_case.input << " is not a misspelling";

    // Check if the suggested words occur.
    for (const std::u16string& suggestion : suggestions) {
      for (const auto& test_case_to_check : kTestCases) {
        int compare_result = suggestion.compare(
            base::ASCIIToUTF16(test_case_to_check.suggestion));
        EXPECT_FALSE(compare_result == 0) << test_case_to_check.suggestion;
      }
    }
  }
}

// Check that the correct dictionary files are checked in.
TEST_F(SpellCheckTest, DictionaryFiles) {
  std::vector<std::string> spellcheck_languages =
      spellcheck::SpellCheckLanguages();
  EXPECT_FALSE(spellcheck_languages.empty());

  base::FilePath hunspell = GetHunspellDirectory();
  for (size_t i = 0; i < spellcheck_languages.size(); ++i) {
    base::FilePath dict =
        spellcheck::GetVersionedFileName(spellcheck_languages[i], hunspell);
    EXPECT_TRUE(base::PathExists(dict)) << dict.value() << " not found";
  }
}

// TODO(groby): Add a test for hunspell itself, when MAXWORDLEN is exceeded.
TEST_F(SpellCheckTest, SpellingEngine_CheckSpelling) {
  static const struct {
    const char* word;
    bool expected_result;
  } kTestCases[] = {
    { "", true },
    { "automatic", true },
    { "hello", true },
    { "forglobantic", false },
    { "xfdssfsdfaasds", false },
    {  // 64 chars are the longest word to check - this should fail checking.
      "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl",
      false
    },
    {  // Any word longer than 64 chars should be exempt from checking.
      "reallylongwordthatabsolutelyexceedsthespecifiedcharacterlimitabit",
      true
    }
  };

  // Initialization magic - call InitializeIfNeeded twice. The first one simply
  // flags internal state that a dictionary was requested. The second one will
  // take the passed-in file and initialize hunspell with it. (The file was
  // passed to hunspell in the ctor for the test fixture).
  // This needs to be done since we need to ensure the SpellingEngine objects
  // contained in the SpellcheckLanguages in |languages_| from the test fixture
  // does get initialized.
  // TODO(groby): Clean up this mess.
  InitializeIfNeeded();
  ASSERT_FALSE(InitializeIfNeeded());

  for (auto& test_case : kTestCases) {
    bool result = CheckSpelling(test_case.word);
    EXPECT_EQ(test_case.expected_result, result)
        << "Failed test for " << test_case.word;
  }
}

// Chrome should not suggest "Othello" for "hellllo" or "identically" for
// "accidently".
TEST_F(SpellCheckTest, LogicalSuggestions) {
  static const struct {
    const char* misspelled;
    const char* suggestion;
  } kTestCases[] = {
    { "hellllo", "hello" },
    { "accidently", "accidentally" }
  };

  for (auto& test_case : kTestCases) {
    size_t misspelling_start = 0;
    size_t misspelling_length = 0;
    std::vector<std::u16string> suggestions;
    EXPECT_FALSE(spell_check()->SpellCheckWord(
        base::ASCIIToUTF16(test_case.misspelled), provider_.GetSpellCheckHost(),
        &misspelling_start, &misspelling_length, &suggestions));
    ASSERT_GE(suggestions.size(), 1U);
    EXPECT_EQ(suggestions[0], base::ASCIIToUTF16(test_case.suggestion));
  }
}

// Words with apostrophes should be valid contractions.
TEST_F(SpellCheckTest, IsValidContraction) {
  static constexpr const char* kLanguages[] = {
      "en-AU", "en-CA", "en-GB", "en-US",
  };

  static constexpr const wchar_t* kWords[] = {
      L"in'n'out",
      L"in’n’out",
  };

  for (auto* const language : kLanguages) {
    ReinitializeSpellCheck(language);
    for (auto* const word : kWords) {
      EXPECT_TRUE(IsValidContraction(base::WideToUTF16(word)));
    }
  }
}

TEST_F(SpellCheckTest, FillSuggestions_OneLanguageNoSuggestions) {
  std::vector<std::vector<std::u16string>> suggestions_list;
  std::vector<std::u16string> suggestion_results;

  suggestions_list.resize(1);

  FillSuggestions(suggestions_list, &suggestion_results);
  EXPECT_TRUE(suggestion_results.empty());
}

TEST_F(SpellCheckTest, FillSuggestions_OneLanguageFewSuggestions) {
  std::vector<std::vector<std::u16string>> suggestions_list;
  std::vector<std::u16string> suggestion_results;

  suggestions_list.resize(1);
  suggestions_list[0].push_back(u"foo");

  FillSuggestions(suggestions_list, &suggestion_results);
  ASSERT_EQ(1U, suggestion_results.size());
  EXPECT_EQ(u"foo", suggestion_results[0]);
}

TEST_F(SpellCheckTest, FillSuggestions_OneLanguageManySuggestions) {
  std::vector<std::vector<std::u16string>> suggestions_list;
  std::vector<std::u16string> suggestion_results;

  suggestions_list.resize(1);
  for (int i = 0; i < spellcheck::kMaxSuggestions + 2; ++i)
    suggestions_list[0].push_back(base::ASCIIToUTF16(base::NumberToString(i)));

  FillSuggestions(suggestions_list, &suggestion_results);
  ASSERT_EQ(static_cast<size_t>(spellcheck::kMaxSuggestions),
            suggestion_results.size());
  for (int i = 0; i < spellcheck::kMaxSuggestions; ++i)
    EXPECT_EQ(base::ASCIIToUTF16(base::NumberToString(i)),
              suggestion_results[i]);
}

TEST_F(SpellCheckTest, FillSuggestions_RemoveDuplicates) {
  std::vector<std::vector<std::u16string>> suggestions_list;
  std::vector<std::u16string> suggestion_results;

  suggestions_list.resize(2);
  for (size_t i = 0; i < 2; ++i) {
    suggestions_list[i].push_back(u"foo");
    suggestions_list[i].push_back(u"bar");
    suggestions_list[i].push_back(u"baz");
  }

  FillSuggestions(suggestions_list, &suggestion_results);
  ASSERT_EQ(3U, suggestion_results.size());
  EXPECT_EQ(u"foo", suggestion_results[0]);
  EXPECT_EQ(u"bar", suggestion_results[1]);
  EXPECT_EQ(u"baz", suggestion_results[2]);
}

TEST_F(SpellCheckTest, FillSuggestions_TwoLanguages) {
  std::vector<std::vector<std::u16string>> suggestions_list;
  std::vector<std::u16string> suggestion_results;

  suggestions_list.resize(2);
  for (size_t i = 0; i < 2; ++i) {
    std::string prefix = base::NumberToString(i);
    suggestions_list[i].push_back(base::ASCIIToUTF16(prefix + "foo"));
    suggestions_list[i].push_back(base::ASCIIToUTF16(prefix + "bar"));
    suggestions_list[i].push_back(base::ASCIIToUTF16(prefix + "baz"));
  }

  // Yes, this test assumes kMaxSuggestions is 5. If it isn't, the test needs
  // to be updated accordingly.
  ASSERT_EQ(5, spellcheck::kMaxSuggestions);
  FillSuggestions(suggestions_list, &suggestion_results);
  ASSERT_EQ(5U, suggestion_results.size());
  EXPECT_EQ(u"0foo", suggestion_results[0]);
  EXPECT_EQ(u"1foo", suggestion_results[1]);
  EXPECT_EQ(u"0bar", suggestion_results[2]);
  EXPECT_EQ(u"1bar", suggestion_results[3]);
  EXPECT_EQ(u"0baz", suggestion_results[4]);
}

TEST_F(SpellCheckTest, FillSuggestions_ThreeLanguages) {
  std::vector<std::vector<std::u16string>> suggestions_list;
  std::vector<std::u16string> suggestion_results;

  suggestions_list.resize(3);
  for (size_t i = 0; i < 3; ++i) {
    std::string prefix = base::NumberToString(i);
    suggestions_list[i].push_back(base::ASCIIToUTF16(prefix + "foo"));
    suggestions_list[i].push_back(base::ASCIIToUTF16(prefix + "bar"));
    suggestions_list[i].push_back(base::ASCIIToUTF16(prefix + "baz"));
  }

  // Yes, this test assumes kMaxSuggestions is 5. If it isn't, the test needs
  // to be updated accordingly.
  ASSERT_EQ(5, spellcheck::kMaxSuggestions);
  FillSuggestions(suggestions_list, &suggestion_results);
  ASSERT_EQ(5U, suggestion_results.size());
  EXPECT_EQ(u"0foo", suggestion_results[0]);
  EXPECT_EQ(u"1foo", suggestion_results[1]);
  EXPECT_EQ(u"2foo", suggestion_results[2]);
  EXPECT_EQ(u"0bar", suggestion_results[3]);
  EXPECT_EQ(u"1bar", suggestion_results[4]);
}
