// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_INLINE_AUTOCOMPLETION_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_INLINE_AUTOCOMPLETION_UTIL_H_

#include <stddef.h>

#include <vector>
#include "base/strings/string16.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "ui/gfx/range/range.h"

// Finds the first occurrence of |search| at a wordbreak within |text| starting
// at |search_start|.
size_t FindAtWordbreak(const base::string16& text,
                       const base::string16& search,
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
    const base::string16& text,
    const base::string16& search);

// Inverts and reverses |ranges| in a domain of [0, |length|). Ranges are
// interpreted as {start, end}. E.g., if |length| is 10 and |ranges| are
// {{2, 3} {5, 9}}, th |InvertRanges| will return {{10, 9}, {5, 3}, {2, 0}}.
// Assumes |ranges| is in forward order; i.e. |ranges[i+1]| occurs after
// |ranges[i]| and |ranges[i].second| after |ranges[i].first|.
std::vector<gfx::Range> InvertAndReverseRanges(
    size_t length,
    std::vector<std::pair<size_t, size_t>> ranges);

#endif  // COMPONENTS_OMNIBOX_BROWSER_INLINE_AUTOCOMPLETION_UTIL_H_
