// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/tokenized_string.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/string_matching/term_break_iterator.h"

namespace ash::string_matching {

using ::base::i18n::BreakIterator;

TokenizedString::TokenizedString(std::u16string text, Mode mode)
    : text_(std::move(text)) {
  switch (mode) {
    case Mode::kCamelCase:
      Tokenize();
      break;
    case Mode::kWords:
      TokenizeWords();
      break;
    default:
      break;
  }
}

TokenizedString::~TokenizedString() = default;

void TokenizedString::Tokenize() {
  BreakIterator break_iter(text_, BreakIterator::BREAK_WORD);
  if (!break_iter.Init()) {
    NOTREACHED_IN_MIGRATION()
        << "BreakIterator init failed" << ", text=\"" << text_ << "\"";
    return;
  }

  while (break_iter.Advance()) {
    if (!break_iter.IsWord())
      continue;

    const std::u16string word(break_iter.GetString());
    const size_t word_start = break_iter.prev();
    TermBreakIterator term_iter(word);
    while (term_iter.Advance()) {
      tokens_.emplace_back(base::i18n::ToLower(term_iter.GetCurrentTerm()));
      mappings_.emplace_back(word_start + term_iter.prev(),
                             word_start + term_iter.pos());
    }
  }
}

void TokenizedString::TokenizeWords() {
  BreakIterator break_iter(text_, BreakIterator::BREAK_WORD);
  if (!break_iter.Init()) {
    NOTREACHED_IN_MIGRATION()
        << "BreakIterator init failed" << ", text=\"" << text_ << "\"";
    return;
  }

  // The token to be generated will be in [start, end) of |text_|.
  size_t start = 0;
  size_t end = 0;
  while (break_iter.Advance()) {
    if (break_iter.IsWord()) {
      // Update |end| but do not generate a token yet because the next segment
      // after Advance may be a non-whitespace char. We may include the next
      // char in the token.
      end = break_iter.pos();
      continue;
    }

    // If this is not a word, it may be a sequence of whitespace chars or
    // another punctuation.
    // 1. Whitespace chars only: generate a token from |text_| in the range of
    //    [start, end). Also reset |start| and |end| for next token.
    // 2. A punctuation: do nothing and Advance.
    const std::u16string word(break_iter.GetString());
    const bool only_whitechars =
        base::ContainsOnlyChars(word, base::kWhitespaceUTF16);
    if (only_whitechars) {
      if (end - start > 1) {
        tokens_.emplace_back(
            base::i18n::ToLower(text_.substr(start, end - start)));
        mappings_.emplace_back(start, end);
      }
      start = break_iter.pos();
      end = start;
    }
  }

  // Generate the last token.
  if (end - start >= 1) {
    tokens_.emplace_back(base::i18n::ToLower(text_.substr(start, end - start)));
    mappings_.emplace_back(start, end);
  }
}

}  //  namespace ash::string_matching
