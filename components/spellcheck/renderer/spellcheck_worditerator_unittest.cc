// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/spellcheck/renderer/spellcheck_worditerator.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/i18n/break_iterator.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::i18n::BreakIterator;
using WordIteratorStatus = SpellcheckWordIterator::WordIteratorStatus;

namespace {

struct TestCase {
    const char* language;
    bool allow_contraction;
    const wchar_t* expected_words;
};

std::u16string GetRulesForLanguage(const std::string& language) {
  SpellcheckCharAttribute attribute;
  attribute.SetDefaultLanguage(language);
  return attribute.GetRuleSet(true);
}

WordIteratorStatus GetNextNonSkippableWord(SpellcheckWordIterator* iterator,
                                           std::u16string* word_string,
                                           size_t* word_start,
                                           size_t* word_length) {
  WordIteratorStatus status = SpellcheckWordIterator::IS_SKIPPABLE;
  while (status == SpellcheckWordIterator::IS_SKIPPABLE)
    status = iterator->GetNextWord(word_string, word_start, word_length);
  return status;
}

}  // namespace

// Tests whether or not our SpellcheckWordIterator can extract words used by the
// specified language from a multi-language text.
TEST(SpellcheckWordIteratorTest, SplitWord) {
  // An input text. This text includes words of several languages. (Some words
  // are not separated with whitespace characters.) Our SpellcheckWordIterator
  // should extract the words used by the specified language from this text and
  // normalize them so our spell-checker can check their spellings. If
  // characters are found that are not from the specified language the test
  // skips them.
  const char16_t kTestText[] =
      // Graphic characters
      u"!@#$%^&*()"
      // Latin (including a contraction character and a ligature).
      u"hello:hello a\xFB03x"
      // Greek
      u"\x03B3\x03B5\x03B9\x03AC\x0020\x03C3\x03BF\x03C5"
      // Cyrillic
      u"\x0437\x0434\x0440\x0430\x0432\x0441\x0442\x0432"
      u"\x0443\x0439\x0442\x0435"
      // Hebrew (including niqquds)
      u"\x05e9\x05c1\x05b8\x05dc\x05d5\x05b9\x05dd "
      // Hebrew words with U+0027 and U+05F3
      u"\x05e6\x0027\x05d9\x05e4\x05e1 \x05e6\x05F3\x05d9\x05e4\x05e1 "
      // Hebrew words with U+0022 and U+05F4
      u"\x05e6\x05d4\x0022\x05dc \x05e6\x05d4\x05f4\x05dc "
      // Hebrew words enclosed with ASCII quotes.
      u"\"\x05e6\x05d4\x0022\x05dc\" '\x05e9\x05c1\x05b8\x05dc\x05d5'"
      // Arabic (including vowel marks)
      u"\x0627\x064e\x0644\x0633\x064e\x0651\x0644\x0627\x0645\x064f "
      u"\x0639\x064e\x0644\x064e\x064a\x0652\x0643\x064f\x0645\x0652 "
      // Farsi/Persian (including vowel marks)
      // Make sure \u064b - \u0652 are removed.
      u"\x0647\x0634\x064e\x0631\x062d "
      u"\x0647\x062e\x0648\x0627\x0647 "
      u"\x0650\x062f\x0631\x062f "
      u"\x0631\x0645\x0627\x0646\x0652 "
      u"\x0633\x0631\x0651 "
      u"\x0646\x0646\x064e\x062c\x064f\x0633 "
      u"\x0627\x0644\x062d\x0645\x062f "
      // Also make sure that class "Lm" (the \u0640) is filtered out too.
      u"\x062c\x062c\x0640\x062c\x062c"
      // Hindi
      u"\x0930\x093E\x091C\x0927\x093E\x0928"
      // Thai
      u"\x0e2a\x0e27\x0e31\x0e2a\x0e14\x0e35\x0020\x0e04"
      u"\x0e23\x0e31\x0e1a"
      // Hiraganas
      u"\x3053\x3093\x306B\x3061\x306F"
      // CJKV ideographs
      u"\x4F60\x597D"
      // Hangul Syllables
      u"\xC548\xB155\xD558\xC138\xC694"
      // Full-width latin : Hello
      u"\xFF28\xFF45\xFF4C\xFF4C\xFF4F "
      u"e.g.,";

  // The languages and expected results used in this test.
  static const TestCase kTestCases[] = {
    {
      // English (keep contraction words)
      "en-US", true, L"hello:hello affix Hello e.g"
    }, {
      // English (split contraction words)
      "en-US", false, L"hello hello affix Hello e g"
    }, {
      // Greek
      "el-GR", true,
      L"\x03B3\x03B5\x03B9\x03AC\x0020\x03C3\x03BF\x03C5"
    }, {
      // Russian
      "ru-RU", true,
      L"\x0437\x0434\x0440\x0430\x0432\x0441\x0442\x0432"
      L"\x0443\x0439\x0442\x0435"
    }, {
      // Hebrew
      "he-IL", true,
      L"\x05e9\x05dc\x05d5\x05dd "
      L"\x05e6\x0027\x05d9\x05e4\x05e1 \x05e6\x05F3\x05d9\x05e4\x05e1 "
      L"\x05e6\x05d4\x0022\x05dc \x05e6\x05d4\x05f4\x05dc "
      L"\x05e6\x05d4\x0022\x05dc \x05e9\x05dc\x05d5"
    }, {
      // Arabic
      "ar", true,
      L"\x0627\x0644\x0633\x0644\x0627\x0645 "
      L"\x0639\x0644\x064a\x0643\x0645 "
      // Farsi/Persian
      L"\x0647\x0634\x0631\x062d "
      L"\x0647\x062e\x0648\x0627\x0647 "
      L"\x062f\x0631\x062f "
      L"\x0631\x0645\x0627\x0646 "
      L"\x0633\x0631 "
      L"\x0646\x0646\x062c\x0633 "
      L"\x0627\x0644\x062d\x0645\x062f "
      L"\x062c\x062c\x062c\x062c"
    }, {
      // Hindi
      "hi-IN", true,
      L"\x0930\x093E\x091C\x0927\x093E\x0928"
    }, {
      // Thai
      "th-TH", true,
      L"\x0e2a\x0e27\x0e31\x0e2a\x0e14\x0e35\x0020\x0e04"
      L"\x0e23\x0e31\x0e1a"
    }, {
      // Korean
      "ko-KR", true,
      L"\x110b\x1161\x11ab\x1102\x1167\x11bc\x1112\x1161"
      L"\x1109\x1166\x110b\x116d"
    },
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestCases[%" PRIuS "]: language=%s", i,
                                    kTestCases[i].language));

    SpellcheckCharAttribute attributes;
    attributes.SetDefaultLanguage(kTestCases[i].language);

    std::u16string input(kTestText);
    SpellcheckWordIterator iterator;
    EXPECT_TRUE(iterator.Initialize(&attributes,
                                    kTestCases[i].allow_contraction));
    EXPECT_TRUE(iterator.SetText(input));

    std::vector<std::u16string> expected_words = base::SplitString(
        base::WideToUTF16(kTestCases[i].expected_words), std::u16string(1, ' '),
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    std::u16string actual_word;
    size_t actual_start, actual_len;
    size_t index = 0;
    for (SpellcheckWordIterator::WordIteratorStatus status =
             iterator.GetNextWord(&actual_word, &actual_start, &actual_len);
         status != SpellcheckWordIterator::IS_END_OF_TEXT;
         status =
             iterator.GetNextWord(&actual_word, &actual_start, &actual_len)) {
      if (status == SpellcheckWordIterator::WordIteratorStatus::IS_SKIPPABLE)
        continue;

      EXPECT_TRUE(index < expected_words.size());
      if (index < expected_words.size())
        EXPECT_EQ(expected_words[index], actual_word);
      ++index;
    }
  }
}

// Tests whether our SpellcheckWordIterator extracts an empty word without
// getting stuck in an infinite loop when inputting a Khmer text. (This is a
// regression test for Issue 46278.)
TEST(SpellcheckWordIteratorTest, RuleSetConsistency) {
  SpellcheckCharAttribute attributes;
  attributes.SetDefaultLanguage("en-US");

  const char16_t kTestText[] = u"\x1791\x17c1\x002e";
  std::u16string input(kTestText);

  SpellcheckWordIterator iterator;
  EXPECT_TRUE(iterator.Initialize(&attributes, true));
  EXPECT_TRUE(iterator.SetText(input));

  // When SpellcheckWordIterator uses an inconsistent ICU ruleset, the following
  // iterator.GetNextWord() calls get stuck in an infinite loop. Therefore, this
  // test succeeds if this call returns without timeouts.
  std::u16string actual_word;
  size_t actual_start, actual_len;
  WordIteratorStatus status = GetNextNonSkippableWord(
      &iterator, &actual_word, &actual_start, &actual_len);

  EXPECT_EQ(SpellcheckWordIterator::WordIteratorStatus::IS_END_OF_TEXT, status);
  EXPECT_EQ(0u, actual_start);
  EXPECT_EQ(0u, actual_len);
}

// Vertify our SpellcheckWordIterator can treat ASCII numbers as word characters
// on LTR languages. On the other hand, it should not treat ASCII numbers as
// word characters on RTL languages because they change the text direction from
// RTL to LTR.
TEST(SpellcheckWordIteratorTest, TreatNumbersAsWordCharacters) {
  // A set of a language, a dummy word, and a text direction used in this test.
  // For each language, this test splits a dummy word, which consists of ASCII
  // numbers and an alphabet of the language, into words. When ASCII numbers are
  // treated as word characters, the split word becomes equal to the dummy word.
  // Otherwise, the split word does not include ASCII numbers.
  static const struct {
    const char* language;
    const wchar_t* text;
    bool left_to_right;
  } kTestCases[] = {
    {
      // English
      "en-US", L"0123456789" L"a", true,
    }, {
      // Greek
      "el-GR", L"0123456789" L"\x03B1", true,
    }, {
      // Russian
      "ru-RU", L"0123456789" L"\x0430", true,
    }, {
      // Hebrew
      "he-IL", L"0123456789" L"\x05D0", false,
    }, {
      // Arabic
      "ar",  L"0123456789" L"\x0627", false,
    }, {
      // Hindi
      "hi-IN", L"0123456789" L"\x0905", true,
    }, {
      // Thai
      "th-TH", L"0123456789" L"\x0e01", true,
    }, {
      // Korean
      "ko-KR", L"0123456789" L"\x1100\x1161", true,
    },
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestCases[%" PRIuS "]: language=%s", i,
                                    kTestCases[i].language));

    SpellcheckCharAttribute attributes;
    attributes.SetDefaultLanguage(kTestCases[i].language);

    std::u16string input_word(base::WideToUTF16(kTestCases[i].text));
    SpellcheckWordIterator iterator;
    EXPECT_TRUE(iterator.Initialize(&attributes, true));
    EXPECT_TRUE(iterator.SetText(input_word));

    std::u16string actual_word;
    size_t actual_start, actual_len;
    WordIteratorStatus status = GetNextNonSkippableWord(
        &iterator, &actual_word, &actual_start, &actual_len);

    EXPECT_EQ(SpellcheckWordIterator::WordIteratorStatus::IS_WORD, status);
    if (kTestCases[i].left_to_right)
      EXPECT_EQ(input_word, actual_word);
    else
      EXPECT_NE(input_word, actual_word);
  }
}

// Verify SpellcheckWordIterator treats typographical apostrophe as a part of
// the word.
TEST(SpellcheckWordIteratorTest, TypographicalApostropheIsPartOfWord) {
  static const struct {
    const char* language;
    const wchar_t* input;
    const wchar_t* expected;
  } kTestCases[] = {
      // Typewriter apostrophe:
      {"en-AU", L"you're", L"you're"},
      {"en-CA", L"you're", L"you're"},
      {"en-GB", L"you're", L"you're"},
      {"en-US", L"you're", L"you're"},
      {"en-US", L"!!!!you're", L"you're"},
      // Typographical apostrophe:
      {"en-AU", L"you\x2019re", L"you\x2019re"},
      {"en-CA", L"you\x2019re", L"you\x2019re"},
      {"en-GB", L"you\x2019re", L"you\x2019re"},
      {"en-US", L"you\x2019re", L"you\x2019re"},
      {"en-US", L"....you\x2019re", L"you\x2019re"},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SpellcheckCharAttribute attributes;
    attributes.SetDefaultLanguage(kTestCases[i].language);

    std::u16string input_word(base::WideToUTF16(kTestCases[i].input));
    std::u16string expected_word(base::WideToUTF16(kTestCases[i].expected));
    SpellcheckWordIterator iterator;
    EXPECT_TRUE(iterator.Initialize(&attributes, true));
    EXPECT_TRUE(iterator.SetText(input_word));

    std::u16string actual_word;
    size_t actual_start, actual_len;
    WordIteratorStatus status = GetNextNonSkippableWord(
        &iterator, &actual_word, &actual_start, &actual_len);

    EXPECT_EQ(SpellcheckWordIterator::WordIteratorStatus::IS_WORD, status);
    EXPECT_EQ(expected_word, actual_word);
    EXPECT_LE(0u, actual_start);
    EXPECT_EQ(expected_word.length(), actual_len);
  }
}

TEST(SpellcheckWordIteratorTest, Initialization) {
  // Test initialization works when a default language is set.
  {
    SpellcheckCharAttribute attributes;
    attributes.SetDefaultLanguage("en-US");

    SpellcheckWordIterator iterator;
    EXPECT_TRUE(iterator.Initialize(&attributes, true));
  }

  // Test initialization fails when no default language is set.
  {
    SpellcheckCharAttribute attributes;

    SpellcheckWordIterator iterator;
    EXPECT_FALSE(iterator.Initialize(&attributes, true));
  }
}

// This test uses English rules to check that different character set
// combinations properly find word breaks and skippable characters.
TEST(SpellcheckWordIteratorTest, FindSkippableWordsEnglish) {
  // A string containing the English word "foo", followed by two Khmer
  // characters, the English word "Can", and then two Russian characters and
  // punctuation.
  std::u16string text(u"foo \x1791\x17C1 Can \x041C\x0438...");
  BreakIterator iter(text, GetRulesForLanguage("en-US"));
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds "foo".
  EXPECT_EQ(u"foo", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space and then the Khmer characters.
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"\x1791\x17C1", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the next space and "Can".
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"Can", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the next space and each Russian character.
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"\x041C", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"\x0438", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the periods at the end.
  EXPECT_EQ(u".", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u".", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u".", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_FALSE(iter.Advance());
}

// This test uses Russian rules to check that different character set
// combinations properly find word breaks and skippable characters.
TEST(SpellcheckWordIteratorTest, FindSkippableWordsRussian) {
  // A string containing punctuation followed by two Russian characters, the
  // English word "Can", and then two Khmer characters.
  std::u16string text(u".;\x041C\x0438 Can \x1791\x17C1  ");
  BreakIterator iter(text, GetRulesForLanguage("ru-RU"));
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds the period and semicolon.
  EXPECT_EQ(u".", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u";", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds all the Russian characters.
  EXPECT_EQ(u"\x041C\x0438", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space and each character in "Can".
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"C", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"a", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"n", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the next space, the Khmer characters, and the last two spaces.
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"\x1791\x17C1", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_FALSE(iter.Advance());
}

// This test uses Khmer rules to check that different character set combinations
// properly find word breaks and skippable characters. Khmer does not use spaces
// between words and uses a dictionary to determine word breaks instead.
TEST(SpellcheckWordIteratorTest, FindSkippableWordsKhmer) {
  // A string containing two Russian characters followed by two, three, and
  // two-character Khmer words, and then English characters and punctuation.
  std::u16string text(
      u"\x041C\x0438 \x178F\x17BE\x179B\x17C4\x1780\x1798\x1780zoo. ,");
  BreakIterator iter(text, GetRulesForLanguage("km"));
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds each Russian character and the space.
  EXPECT_EQ(u"\x041C", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"\x0438", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the first two-character Khmer word.
  EXPECT_EQ(u"\x178F\x17BE", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the three-character Khmer word and then the next two-character word.
  // Note: Technically these are two different Khmer words so the Khmer language
  // rule should find a break between them but due to the heuristic/statistical
  // nature of the Khmer word breaker it does not.
  EXPECT_EQ(u"\x179B\x17C4\x1780\x1798\x1780", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds each character in "zoo".
  EXPECT_EQ(u"z", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"o", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"o", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds the period, space, and comma.
  EXPECT_EQ(u".", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u",", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_FALSE(iter.Advance());
}

TEST(SpellcheckCharAttributeTest, IsTextInSameScript) {
  struct LanguageWithSampleText {
    const char* language;
    const wchar_t* sample_text;
  };

  static const std::vector<LanguageWithSampleText> kLanguagesWithSampleText = {
      // Latin
      {"fr", L"Libert\x00e9, \x00e9galitt\x00e9, fraternit\x00e9."},
      // Greek
      {"el", L"\x03B3\x03B5\x03B9\x03AC\x0020\x03C3\x03BF\x03C5"},
      // Cyrillic
      {"ru",
       L"\x0437\x0434\x0440\x0430\x0432\x0441\x0442\x0432\x0443\x0439\x0442"
       L"\x0435"},
      // Hebrew
      {"he", L"\x05e9\x05c1\x05b8\x05dc\x05d5\x05b9\x05dd"},
      // Arabic
      {"ar",
       L"\x0627\x064e\x0644\x0633\x064e\x0651\x0644\x0627\x0645\x064f\x0639"
       L"\x064e\x0644\x064e\x064a\x0652\x0643\x064f\x0645\x0652 "},
      // Hindi
      {"hi", L"\x0930\x093E\x091C\x0927\x093E\x0928"},
      // Thai
      {"th",
       L"\x0e2a\x0e27\x0e31\x0e2a\x0e14\x0e35\x0020\x0e04\x0e23\x0e31\x0e1a"},
      // Hiragata
      {"jp-Hira", L"\x3053\x3093\x306B\x3061\x306F"},
      // Katakana
      {"jp-Kana", L"\x30b3\x30de\x30fc\x30b9"},
      // CJKV ideographs
      {"zh-Hani", L"\x4F60\x597D"},
      // Hangul Syllables
      {"ko", L"\xC548\xB155\xD558\xC138\xC694"},
  };

  for (const auto& testcase : kLanguagesWithSampleText) {
    SpellcheckCharAttribute attribute;
    attribute.SetDefaultLanguage(testcase.language);
    std::u16string sample_text(base::WideToUTF16(testcase.sample_text));
    EXPECT_TRUE(attribute.IsTextInSameScript(sample_text))
        << "Language \"" << testcase.language
        << "\" fails to identify that sample text in same language is in same "
           "script.";

    // All other scripts in isolatation or mixed with current script should
    // return false.
    for (const auto& other_script : kLanguagesWithSampleText) {
      if (testcase.language == other_script.language)
        continue;
      std::u16string other_sample_text(
          base::WideToUTF16(other_script.sample_text));
      EXPECT_FALSE(attribute.IsTextInSameScript(other_sample_text))
          << "Language \"" << testcase.language
          << "\" fails to identify that sample text in language \""
          << other_script.language << "\" is in different script.";
      EXPECT_FALSE(
          attribute.IsTextInSameScript(sample_text + other_sample_text))
          << "Language \"" << testcase.language
          << "\" fails to identify that sample text in language \""
          << other_script.language
          << "\" is in different script when appended to text in this script.";
      EXPECT_FALSE(
          attribute.IsTextInSameScript(other_sample_text + sample_text))
          << "Language \"" << testcase.language
          << "\" fails to identify that sample text in language \""
          << other_script.language
          << "\" is in different script when prepended to text in this script.";
    }
  }
}
