// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TAILORED_WORD_BREAK_ITERATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_TAILORED_WORD_BREAK_ITERATOR_H_

#include "base/i18n/break_iterator.h"

// Breaks on an underscore. Otherwise, it behaves like its parent class with
// BreakIterator::BREAK_WORD.
class TailoredWordBreakIterator : public base::i18n::BreakIterator {
 public:
  TailoredWordBreakIterator(const base::StringPiece16& str,
                            base::i18n::BreakIterator::BreakType break_type);

  ~TailoredWordBreakIterator();
  TailoredWordBreakIterator(const TailoredWordBreakIterator&) = delete;
  TailoredWordBreakIterator& operator=(const TailoredWordBreakIterator&) =
      delete;

  bool Advance();
  bool IsWord() const;
  // Returns characters between |prev_| and |pos_| if |underscore_word_| is not
  // empty. Otherwise returns the normal BreakIterator-determined current word.
  base::StringPiece16 GetStringPiece() const;
  std::u16string GetString() const;
  size_t prev() const;
  size_t pos() const;

 private:
  // Returns true if we processing a word with underscores (i.e., |pos| points
  // to a valid position in |underscore_word_|).
  bool HasUnderscoreWord() const;
  // Updates |prev_| and |pos_| considering underscore. Returns true if we
  // successfully advanced within the underscore word, and returns false if
  // we've exhausted the underscore word, and we should resume the main word
  // traversal. This is similar to the semantics of `BreakIterator::Advance()`.
  bool AdvanceInUnderscoreWord();
  // |prev_| and |pos_| are indices to |underscore_word_|.
  size_t prev_, pos_;
  // Set if BreakIterator::GetStringPiece() contains '_', otherwise it's empty.
  base::StringPiece16 underscore_word_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TAILORED_WORD_BREAK_ITERATOR_H_
