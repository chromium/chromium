// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/tailored_word_break_iterator.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/i18n/break_iterator.h"

using base::i18n::BreakIterator;

TailoredWordBreakIterator::TailoredWordBreakIterator(std::u16string_view str)
    : BreakIterator(str, BreakIterator::BREAK_WORD),
      prev_(0),
      pos_(0),
      word_breaks_{u"0123456789"},
      non_word_breaks_{u"_"},
      all_breaks_{word_breaks_ + non_word_breaks_} {}

TailoredWordBreakIterator::~TailoredWordBreakIterator() = default;

bool TailoredWordBreakIterator::Advance() {
  if (HasSpecialWord() && AdvanceInSpecialWord()) {
    return true;
  }
  if (!BreakIterator::Advance())
    return false;
  prev_ = 0;
  pos_ = 0;
  special_word_ = std::u16string_view();
  if (!IsWord())
    return true;
  std::u16string_view word = BreakIterator::GetStringView();
  if (word.find_first_of(all_breaks_) != std::u16string_view::npos) {
    special_word_ = word;
    AdvanceInSpecialWord();
  }
  return true;
}

bool TailoredWordBreakIterator::IsWord() const {
  if (HasSpecialWord()) {
    std::u16string_view word = GetStringView();
    if (!word.empty())
      return non_word_breaks_.find(word[0]) == std::u16string::npos;
  }
  return BreakIterator::IsWord();
}

std::u16string_view TailoredWordBreakIterator::GetStringView() const {
  if (!special_word_.empty())
    return special_word_.substr(prev_, pos_ - prev_);
  return BreakIterator::GetStringView();
}

std::u16string TailoredWordBreakIterator::GetString() const {
  return std::u16string(GetStringView());
}

size_t TailoredWordBreakIterator::prev() const {
  return BreakIterator::prev() + prev_;
}

size_t TailoredWordBreakIterator::pos() const {
  return BreakIterator::pos() + pos_;
}

bool TailoredWordBreakIterator::HasSpecialWord() const {
  return !special_word_.empty();
}

bool TailoredWordBreakIterator::AdvanceInSpecialWord() {
  DCHECK(HasSpecialWord());
  // If we've finished with the special word we're processing, return false
  // and let the caller call advance on the outer `BreakIterator`.
  if (pos_ == special_word_.size()) {
    prev_ = 0;
    pos_ = 0;
    special_word_ = std::u16string_view();
    return false;
  }

  prev_ = pos_;
  auto c = special_word_[pos_];

  if (non_word_breaks_.find(c) != std::u16string::npos) {
    // If at a non-word word-break (e.g. '_'), advance 1 char. Don't advance to
    // the end of the word-break series to be consistent with how
    // `BreakIterator` handles other symbols.
    pos_++;

  } else if (word_breaks_.find(c) != std::u16string::npos) {
    // If at a word word-break (e.g. numbers), advance to the end of the series.
    pos_ = special_word_.find_first_not_of(word_breaks_, pos_ + 1);

  } else {
    // Otherwise, at a non-word-break, advance to the next word-break.
    pos_ = special_word_.find_first_of(all_breaks_, pos_ + 1);
  }

  if (pos_ == std::u16string::npos)
    pos_ = special_word_.size();
  return true;
}
