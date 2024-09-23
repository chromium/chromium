// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TAILORED_WORD_BREAK_ITERATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_TAILORED_WORD_BREAK_ITERATOR_H_

#include <string>
#include <string_view>

#include "base/i18n/break_iterator.h"

// Breaks on an underscore and numbers. Otherwise, it behaves like its parent
// class with `BreakIterator::BREAK_WORD`.
// E.g. 'Viktor Ambartsumian_is__anAwesome99_99Astrophysicist!!' is broken into:
// [Viktor, <space>, Ambartsumian, _, is, _, _, anAwesome, 99, _, 99,
//  Astrophysicist, !, !].
class TailoredWordBreakIterator : public base::i18n::BreakIterator {
 public:
  explicit TailoredWordBreakIterator(std::u16string_view str);

  ~TailoredWordBreakIterator();
  TailoredWordBreakIterator(const TailoredWordBreakIterator&) = delete;
  TailoredWordBreakIterator& operator=(const TailoredWordBreakIterator&) =
      delete;

  bool Advance();
  bool IsWord() const;
  // Returns characters between `prev_` and `pos_` if `special_word_` is not
  // empty. Otherwise returns the normal `BreakIterator`-determined current
  // word.
  std::u16string_view GetStringView() const;
  std::u16string GetString() const;
  size_t prev() const;
  size_t pos() const;

 private:
  // Returns true if processing a word with underscores or numbers (i.e., `pos`
  // points to a valid position in `special_word_`).
  bool HasSpecialWord() const;

  // Updates `prev_` and `pos_` considering underscores and numbers. Returns
  // true if it successfully advanced within `special_word_`; returns false if
  // it exhausts the word and should resume the main word traversal. This is
  // similar to the semantics of `BreakIterator::Advance()`.
  bool AdvanceInSpecialWord();

  // `prev_` and `pos_` are indices to `special_word_`.
  size_t prev_, pos_;
  // Set if `BreakIterator::GetStringView()` contains '_' or numbers, otherwise
  // it's empty.
  std::u16string_view special_word_;

  // The additional chars to break on that aren't broken on by `BreakIterator`.
  // Subset of `all_breaks_` that return true from `IsWord()` (e.g. numbers).
  const std::u16string word_breaks_;
  // Subset of `all_breaks_` that return false from `IsWord()` (e.g.
  // underscore).
  const std::u16string non_word_breaks_;
  // Union of `word_breaks_` & `word_breaks_` (e.g. numbers & underscore).
  const std::u16string all_breaks_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TAILORED_WORD_BREAK_ITERATOR_H_
