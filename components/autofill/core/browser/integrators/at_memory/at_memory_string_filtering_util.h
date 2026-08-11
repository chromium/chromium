// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_STRING_FILTERING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_STRING_FILTERING_UTIL_H_

#include <string_view>

namespace autofill {

// Returns true if `target_token` fuzzy-matches `query_token` (either as a full
// token match or prefix match) within an allowed Levenshtein edit distance
// based on the length of `query_token`.
[[nodiscard]] bool FuzzyMatchesSingleToken(std::u16string_view target_token,
                                           std::u16string_view query_token);

// Tokenizes `normalized_target` and `normalized_filter` into words and returns
// true if all query tokens fuzzy-match target tokens in an ordered sequence
// (subsequence matching). Expects inputs to already be normalized.
[[nodiscard]] bool FuzzyMatchesOrderedTokens(
    std::u16string_view normalized_target,
    std::u16string_view normalized_filter);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_STRING_FILTERING_UTIL_H_
