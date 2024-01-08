// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_TERM_BREAK_ITERATOR_H_
#define CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_TERM_BREAK_ITERATOR_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"

namespace base::i18n {
class UTF16CharIterator;
}

namespace ash::string_matching {

// TermBreakIterator breaks terms out of a word. Terms are broken on
// camel case boundaries and alpha/number boundaries. Numbers are defined
// as [0-9\.,]+.
//  e.g.
//   CamelCase -> Camel, Case
//   Python2.7 -> Python, 2.7
class TermBreakIterator {
 public:
  // Note that |word| must out live this iterator.
  explicit TermBreakIterator(const std::u16string& word);

  TermBreakIterator(const TermBreakIterator&) = delete;
  TermBreakIterator& operator=(const TermBreakIterator&) = delete;

  ~TermBreakIterator();

  // Advance to the next term. Returns false if at the end of the word.
  bool Advance();

  // Returns the current term, which is the substr of |word_| in range
  // [prev_, pos_).
  const std::u16string GetCurrentTerm() const;

  size_t prev() const { return prev_; }
  size_t pos() const { return pos_; }

  static const size_t npos = static_cast<size_t>(-1);

 private:
  enum State {
    STATE_START,   // Initial state
    STATE_NUMBER,  // Current char is a number [0-9\.,].
    STATE_UPPER,   // Current char is upper case.
    STATE_LOWER,   // Current char is lower case.
    STATE_CHAR,    // Current char has no case, e.g. a cjk char.
    STATE_LAST,
  };

  // Returns new state for given |ch|.
  State GetNewState(char16_t ch);

  const raw_ref<const std::u16string> word_;
  size_t prev_;
  size_t pos_;

  std::unique_ptr<base::i18n::UTF16CharIterator> iter_;
  State state_;
};

}  // namespace ash::string_matching

#endif  // CHROMEOS_ASH_COMPONENTS_STRING_MATCHING_TERM_BREAK_ITERATOR_H_
