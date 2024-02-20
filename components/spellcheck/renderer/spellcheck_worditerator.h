// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines an iterator class that enumerates words supported by our spellchecker
// from multi-language text. This class is used for filtering out characters
// not supported by our spellchecker.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_WORDITERATOR_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_WORDITERATOR_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "third_party/icu/source/common/unicode/uscript.h"

namespace base {
namespace i18n {
class BreakIterator;
} // namespace i18n
} // namespace base

// A class which encapsulates language-specific operations used by
// SpellcheckWordIterator. When we set the spellchecker language, this class
// creates rule sets that filter out the characters not supported by the
// spellchecker. (Please read the comment in the SpellcheckWordIterator class
// about how to use this class.)
class SpellcheckCharAttribute {
 public:
  SpellcheckCharAttribute();

  SpellcheckCharAttribute(const SpellcheckCharAttribute&) = delete;
  SpellcheckCharAttribute& operator=(const SpellcheckCharAttribute&) = delete;

  ~SpellcheckCharAttribute();

  // Sets the language of the spellchecker. When this function is called with an
  // ISO language code, this function creates the custom rule-sets used by
  // the ICU break iterator so it can extract only words used by the language.
  // GetRuleSet() returns the rule-sets created in this function.
  void SetDefaultLanguage(const std::string& language);

  // Returns true if all the characters in a text string are in the script
  // associated with the spellcheck language.
  bool IsTextInSameScript(const std::u16string& text) const;

  // Returns a custom rule-set string used by the ICU break iterator. This class
  // has two rule-sets, one splits a contraction and the other does not, so we
  // can split a concaticated word (e.g. "seven-year-old") into words (e.g.
  // "seven", "year", and "old") and check their spellings. The result stirng is
  // encoded in UTF-16 since ICU needs UTF-16 strings.
  std::u16string GetRuleSet(bool allow_contraction) const;

  // Outputs a character only if it is a word character. (Please read the
  // comments in CreateRuleSets() why we need this function.)
  bool OutputChar(UChar c, std::u16string* output) const;

 private:
  // Creates the rule-sets that return words possibly used by the given
  // language. Unfortunately, these rule-sets are not perfect and have some
  // false-positives. For example, they return combined accent marks even though
  // we need English words only. We call OutputCharacter() to filter out such
  // false-positive characters.
  void CreateRuleSets(const std::string& language);

  // Outputs a character only if it is one used by the given language. These
  // functions are called from OutputChar().
  bool OutputArabic(UChar c, std::u16string* output) const;
  bool OutputHangul(UChar c, std::u16string* output) const;
  bool OutputHebrew(UChar c, std::u16string* output) const;
  bool OutputDefault(UChar c, std::u16string* output) const;

  // The custom rule-set strings used by ICU break iterator. Since it is not so
  // easy to create custom rule-sets from an ISO language code, this class
  // saves these rule-set strings created when we set the language.
  std::u16string ruleset_allow_contraction_;
  std::u16string ruleset_disallow_contraction_;

  // The script code used by this language.
  UScriptCode script_code_;
};

// A class which extracts words that can be checked for spelling from a
// multi-language string. The ICU word-break iterator does not discard some
// punctuation characters attached to a word. For example, when we set a word
// "_hello_" to a word-break iterator, it just returns "_hello_". Neither does
// it discard characters not used by the language. For example, it returns
// Russian words even though we need English words only. To extract only the
// words that our spellchecker can check their spellings, this class uses custom
// rule-sets created by the SpellcheckCharAttribute class. Also, this class
// normalizes extracted words so our spellchecker can check the spellings of
// words that include ligatures, combined characters, full-width characters,
// etc. This class uses UTF-16 strings as its input and output strings since
// UTF-16 is the native encoding of ICU and avoid unnecessary conversions
// when changing the encoding of this string for our spellchecker. (Chrome can
// use two or more spellcheckers and we cannot assume their encodings.)
// The following snippet is an example that extracts words with this class.
//
//   // Creates the language-specific attributes for US English.
//   SpellcheckCharAttribute attribute;
//   attribute.SetDefaultLanguage("en-US");
//
//   // Set up a SpellcheckWordIterator object which extracts English words,
//   // and retrieve them.
//   SpellcheckWordIterator iterator;
//   std::u16string text(u"this is a test.");
//   iterator.Initialize(&attribute, true);
//   iterator.SetText(text);
//
//   std::u16string word;
//   int offset;
//   int length;
//   while (iterator.GetNextWord(&word, &offset, &length)) {
//     ...
//   }
//
class SpellcheckWordIterator {
 public:
  enum WordIteratorStatus {
    // The end of a sequence of text that the iterator recognizes as characters
    // that can form a word.
    IS_WORD,
    // Non-word characters that the iterator can skip past, such as punctuation,
    // whitespace, and characters from another character set.
    IS_SKIPPABLE,
    // The end of the text that the iterator is going over.
    IS_END_OF_TEXT
  };

  SpellcheckWordIterator();

  SpellcheckWordIterator(const SpellcheckWordIterator&) = delete;
  SpellcheckWordIterator& operator=(const SpellcheckWordIterator&) = delete;

  ~SpellcheckWordIterator();

  // Initializes a word-iterator object with the language-specific attribute. If
  // we need to split contractions and concatenated words, call this function
  // with its 'allow_contraction' parameter false. (This function uses lots of
  // temporal memory to compile a custom word-break rule into an automaton.)
  bool Initialize(const SpellcheckCharAttribute* attribute,
                  bool allow_contraction);

  // Returns whether this word iterator is initialized.
  bool IsInitialized() const;

  // Set text to be iterated. (This text does not have to be NULL-terminated.)
  // This function also resets internal state so we can reuse this iterator
  // without calling Initialize().
  bool SetText(std::u16string_view text);

  // Advances |iterator_| through |text_| and gets the current status of the
  // word iterator within |text|:
  //
  //  - Returns IS_WORD if the iterator just found the end of a sequence of word
  //    characters and it was able to normalize the sequence. This stores the
  //    normalized string into |word_string| and stores the position and length
  //    into |word_start| and |word_length| respectively. Keep in mind that
  //    since this function normalizes the output word, the length of
  //    |word_string| may be different from the |word_length|. Therefore, when
  //    we call functions that change the input text, such as
  //    string16::replace(), we need to use |word_start| and |word_length| as
  //    listed in the following snippet:
  //
  //      while(iterator.GetNextWord(&word, &offset, &length))
  //        text.replace(offset, length, word);
  //
  //  - Returns IS_SKIPPABLE if the iterator just found a character that the
  //    iterator can skip past such as punctuation, whitespace, and characters
  //    from another character set. This stores the character, position, and
  //    length into |word_string|, |word_start|, and |word_length| respectively.
  //
  //  - Returns IS_END_OF_TEXT if the iterator has reached the end of |text_|.
  SpellcheckWordIterator::WordIteratorStatus GetNextWord(
      std::u16string* word_string,
      size_t* word_start,
      size_t* word_length);

  // Releases all the resources attached to this object.
  void Reset();

 private:
  // Normalizes a non-terminated string returned from an ICU word-break
  // iterator. A word returned from an ICU break iterator may include characters
  // not supported by our spellchecker, e.g. ligatures, combining/ characters,
  // full-width letters, etc. This function replaces such characters with
  // alternative characters supported by our spellchecker. This function also
  // calls SpellcheckWordIterator::OutputChar() to filter out false-positive
  // characters.
  bool Normalize(size_t input_start,
                 size_t input_length,
                 std::u16string* output_string) const;

  // The pointer to the input string from which we are extracting words.
  const char16_t* text_;

  // The language-specific attributes used for filtering out non-word
  // characters.
  raw_ptr<const SpellcheckCharAttribute> attribute_;

  // The break iterator.
  std::unique_ptr<base::i18n::BreakIterator> iterator_;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_WORDITERATOR_H_
