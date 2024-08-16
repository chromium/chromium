// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/string_matching/term_break_iterator.h"

#include <ostream>

#include "base/check.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace ash::string_matching {

TermBreakIterator::TermBreakIterator(const std::u16string& word)
    : word_(word),
      prev_(npos),
      pos_(0),
      iter_(std::make_unique<base::i18n::UTF16CharIterator>(word)),
      state_(STATE_START) {}

TermBreakIterator::~TermBreakIterator() = default;

bool TermBreakIterator::Advance() {
  // 2D matrix that defines term boundaries. Each row represents current state.
  // Each col represents new state from input char. Cells with true value
  // represents a term boundary.
  const bool kBoundary[][STATE_LAST] = {
      // START  NUMBER UPPER  LOWER  CHAR
      {false, false, false, false, false},  // START
      {false, false, true, true, true},     // NUMBER
      {false, true, false, false, true},    // UPPER
      {false, true, true, false, true},     // LOWER
      {false, true, true, true, false},     // CHAR
  };

  while (iter_->Advance()) {
    const State new_state = GetNewState((*word_)[iter_->array_pos()]);
    const bool is_boundary = kBoundary[state_][new_state];
    state_ = new_state;
    if (is_boundary)
      break;
  }

  prev_ = pos_;
  pos_ = iter_->array_pos();

  return prev_ != pos_ || !iter_->end();
}

const std::u16string TermBreakIterator::GetCurrentTerm() const {
  DCHECK(prev_ != npos && pos_ != npos);
  return word_->substr(prev_, pos_ - prev_);
}

TermBreakIterator::State TermBreakIterator::GetNewState(char16_t ch) {
  if (base::IsAsciiDigit(ch) || ch == '.' || ch == ',')
    return STATE_NUMBER;

  const bool is_upper = !!u_isUUppercase(ch);
  const bool is_lower = !!u_isULowercase(ch);

  if (is_upper && is_lower) {
    NOTREACHED_IN_MIGRATION()
        << "Invalid state for ch=" << std::u16string(1, ch);
    return STATE_CHAR;
  }

  if (is_upper)
    return STATE_UPPER;
  if (is_lower)
    return STATE_LOWER;

  return STATE_CHAR;
}

}  // namespace ash::string_matching
