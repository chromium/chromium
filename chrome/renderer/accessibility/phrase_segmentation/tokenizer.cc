// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/tokenizer.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/numerics/safe_conversions.h"

namespace {
bool is_all_spaces(const std::u16string& str) {
  return str.find_first_not_of(' ') == std::string::npos;
}
}  // namespace

std::vector<std::pair<int, int>> Tokenizer::Tokenize(
    const std::u16string& text) {
  std::vector<std::pair<int, int>> ret;
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init()) {
    return ret;
  }

  // iter.Advance() returns false if we've run past end of the text.
  while (iter.Advance()) {
    // Tokenize words as well as punctuations.
    if (iter.IsWord() || !is_all_spaces(iter.GetString())) {
      ret.emplace_back(base::checked_cast<int>(iter.prev()) /* start index */,
                       base::checked_cast<int>(iter.pos()) /* end index */);
    }
  }
  return ret;
}
