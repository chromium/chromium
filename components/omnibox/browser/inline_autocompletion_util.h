// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_INLINE_AUTOCOMPLETION_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_INLINE_AUTOCOMPLETION_UTIL_H_

#include <stddef.h>

#include <string>
#include <vector>
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "ui/gfx/range/range.h"

// Finds the first occurrence of |search| at a wordbreak within |text| starting
// at |search_start|.
size_t FindAtWordbreak(const std::u16string& text,
                       const std::u16string& search,
                       size_t search_start = 0);

// Splits |search| into words and finds them in |text|, returning a vector of
// occurrence starts and ends.
// - Occurrences must be sequential. E.g. 'a c' can be found in 'a b c' but not
//   in 'c b a'.
// - Occurrences must be at word breaks. E.g. 'a c' cannot be found in 'a bc'.
// - Whitespaces must also match. E.g. 'a c' cannot be found in 'a-c' but can
//   be found in 'a -c' and 'a- c'.
// If all words in |search| were not found, then returns an empty vector.
std::vector<std::pair<size_t, size_t>> FindWordsSequentiallyAtWordbreak(
    const std::u16string& text,
    const std::u16string& search);

// Inverts and reverses term |matches| in a domain of [0, |length|) to determine
// the selected non-matches. Ranges are interpreted as {start, end}. E.g., if
// |length| is 10 and |matches| are {{2, 3} {5, 9}}, |InvertRanges| will return
// {{10, 9}, {5, 3}, {2, 0}}. Assumes |matches| is in forward order; i.e.
// |matches[i+1]| occurs after |matches[i]| and |matches[i].second| after
// |matches[i].first|.
std::vector<gfx::Range> TermMatchesToSelections(
    size_t length,
    std::vector<std::pair<size_t, size_t>> matches);

#endif  // COMPONENTS_OMNIBOX_BROWSER_INLINE_AUTOCOMPLETION_UTIL_H_
