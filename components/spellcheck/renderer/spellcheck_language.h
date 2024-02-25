// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_LANGUAGE_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_LANGUAGE_H_

#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "components/spellcheck/renderer/spellcheck_worditerator.h"

namespace service_manager {
class LocalInterfaceProvider;
}

namespace spellcheck::mojom {
class SpellCheckHost;
}

class SpellingEngine;

class SpellcheckLanguage {
 public:
  enum SpellcheckWordResult {
    // Denotes that every recognized word is spelled correctly, from the point
    // of spellchecking to the end of the text.
    IS_CORRECT,
    // A sequence of skippable characters, such as punctuation, spaces, or
    // characters not recognized by the current spellchecking language.
    IS_SKIPPABLE,
    // A misspelled word has been found in the text.
    IS_MISSPELLED
  };

  explicit SpellcheckLanguage(
      service_manager::LocalInterfaceProvider* embedder_provider);

  SpellcheckLanguage(const SpellcheckLanguage&) = delete;
  SpellcheckLanguage& operator=(const SpellcheckLanguage&) = delete;

  ~SpellcheckLanguage();

  void Init(base::File file, const std::string& language);

  // Spellcheck a sequence of text.
  // |text| is the segment of the text being spellchecked. The |tag|
  // parameter should either be a unique identifier for the document that the
  // word came from (if the current platform requires it), or 0.
  //
  // - Returns IS_CORRECT if every word in |text| is recognized and spelled
  //   correctly. Also, returns IS_CORRECT if the spellchecker failed to
  //   initialize.
  //
  // - Returns IS_SKIPPABLE if a sequence of skippable characters, such as
  //   punctuation, spaces, or unrecognized characters, is found.
  //   |skip_or_misspelling_start| and |skip_or_misspelling_len| are then set to
  //   the position and the length of the sequence of skippable characters.
  //
  // - Returns IS_MISSPELLED if a misspelled word is found.
  //   |skip_or_misspelling_start| and |skip_or_misspelling_len| are then set to
  //   the position and length of the misspelled word. In addition, finds the
  //   suggested words for a given misspelled word and puts them into
  //   |*optional_suggestions|. If optional_suggestions is nullptr, suggested
  //   words will not be looked up. Note that doing suggest lookups can be slow.
  SpellcheckWordResult SpellCheckWord(
      std::u16string_view text,
      spellcheck::mojom::SpellCheckHost& host,
      size_t* skip_or_misspelling_start,
      size_t* skip_or_misspelling_len,
      std::vector<std::u16string>* optional_suggestions);

  // Initialize |spellcheck_| if that hasn't happened yet.
  bool InitializeIfNeeded();

  // Return true if the underlying spellcheck engine is enabled.
  bool IsEnabled();

  // Returns true if all the characters in a text string are in the script
  // associated with this spellcheck language.
  bool IsTextInSameScript(const std::u16string& text) const;

 private:
  friend class SpellCheckTest;
  friend class FakeSpellCheck;

  // Returns whether or not the given word is a contraction of valid words
  // (e.g. "word:word").
  bool IsValidContraction(const std::u16string& word,
                          spellcheck::mojom::SpellCheckHost& host);

  // Represents character attributes used for filtering out characters which
  // are not supported by this SpellCheck object.
  SpellcheckCharAttribute character_attributes_;

  // Represents word iterators used in this spellchecker. The |text_iterator_|
  // splits text provided by WebKit into words, contractions, or concatenated
  // words. The |contraction_iterator_| splits a concatenated word extracted by
  // |text_iterator_| into word components so we can treat a concatenated word
  // consisting only of correct words as a correct word.
  SpellcheckWordIterator text_iterator_;
  SpellcheckWordIterator contraction_iterator_;

  // Pointer to a platform-specific spelling engine, if it is in use. This
  // should only be set if hunspell is not used. (I.e. on OSX, for now)
  std::unique_ptr<SpellingEngine> platform_spelling_engine_;
};

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_LANGUAGE_H_
