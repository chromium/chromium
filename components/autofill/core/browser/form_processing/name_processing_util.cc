// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

namespace {

// Common prefixes are only removed if a minimum number of fields are present
// and a sufficiently long prefix is found. These values are chosen to be
// effective against web frameworks which prepend prefixes such as
// "ctl01$ctl00$MainContentRegion$" on all fields.
constexpr size_t kCommonNamePrefixRemovalFieldThreshold = 3;
// Minimum required length for prefixes to be removed.
constexpr size_t kMinCommonNamePrefixLength = 16;

// AutofillLabelAffixRemoval removes prefixes more aggressively by separating
// the strings into intervals of size at least `kMinSizeOfInterval` and common
// prefix of at least `kMinPrefixLengthForInterval`.
// We don't know what good values would be and the current choice is arbitrary.
constexpr size_t kMinSizeOfInterval = 3;
constexpr size_t kMinPrefixLengthForInterval = 5;

// Returns true if `parseable_name` is a valid parseable_name. To be considered
// valid, the string cannot be empty or consist of digits only.
// This condition prevents the logic from simplifying strings like
// "address-line-1", "address-line-2" to "1", "2".
bool IsValidParseableName(base::StringPiece16 parseable_name) {
  static constexpr char16_t kRegex[] = u"\\D";
  return MatchesRegex<kRegex>(parseable_name);
}

// Tries to remove a prefix of length `len` from all `strings`. The removal
// fails if one of the resulting strings is not `IsValidParseableName()`.
// Assumes that all strings are at least `len` long.
void MaybeRemovePrefix(base::span<base::StringPiece16> strings, size_t len) {
  DCHECK(base::ranges::all_of(
      strings, [&](base::StringPiece16 s) { return s.size() >= len; }));
  if (!base::ranges::all_of(strings, [&](base::StringPiece16 s) {
        return IsValidParseableName(s.substr(len));
      })) {
    return;
  }
  for (base::StringPiece16& s : strings)
    s.remove_prefix(len);
}

// Finds the largest prefix of `strings` such that the longest common prefix of
// the strings within that prefix is at least `kMinPrefixLengthForInterval`.
// Returns the size of that prefix and the length of the longest common prefix
// of the strings within that prefix.
// To maintain the interval's lcp with increasing size efficiently, we use the
// following property of lcps:
// Given strings s1...sn,
//   min(lcp(si, sj)) for all i, j = min(lcp(s1, sk)) for all k
std::pair<size_t, size_t> FindEndOfInterval(
    base::span<base::StringPiece16> strings) {
  size_t lcp = std::numeric_limits<size_t>::max();
  for (size_t i = 0; i < strings.size(); i++) {
    size_t current_lcp =
        FindLongestCommonPrefixLength(std::array{strings[0], strings[i]});
    if (current_lcp < kMinPrefixLengthForInterval)
      return {i, lcp};
    lcp = std::min(lcp, current_lcp);
  }
  return {strings.size(), lcp};
}

// Divides `strings` into the smallest number of intervals such that the longest
// common prefix of the strings in each interval is at least
// `kMinPrefixLengthForInterval`. Then removes this common prefix for every
// interval.
// Intervals shorter than `kMinSizeOfInterval` and strings shorter than
// `kMinPrefixLengthForInterval` are left as-is.
// This is useful for removing common prefixes like "shipping-". It cannot be
// done after sectioning, as the field names are required for local heuristics.
void RemoveCommonPrefixInIntervals(base::span<base::StringPiece16> strings) {
  auto it = strings.begin();
  while (it != strings.end()) {
    auto [size, lcp] = FindEndOfInterval({it, strings.end()});
    if (size >= kMinSizeOfInterval) {
      MaybeRemovePrefix({it, size}, lcp);
    } else if (size == 0) {
      size = 1;  // `*it` is smaller than the threshold. Skip it.
    }
    it += size;
  }
}

}  // namespace

size_t FindLongestCommonPrefixLength(
    base::span<const base::StringPiece16> strings) {
  if (strings.empty())
    return 0;

  size_t prefix_len = 0;
  auto AgreeOnNextChar = [&](base::StringPiece16 other) {
    return prefix_len < other.size() &&
           strings[0][prefix_len] == other[prefix_len];
  };
  while (base::ranges::all_of(strings, AgreeOnNextChar))
    ++prefix_len;
  return prefix_len;
}

void ComputeParseableNames(base::span<base::StringPiece16> field_names) {
  if (base::FeatureList::IsEnabled(features::kAutofillLabelAffixRemoval)) {
    RemoveCommonPrefixInIntervals(field_names);
    return;
  }
  if (field_names.size() < kCommonNamePrefixRemovalFieldThreshold)
    return;
  size_t lcp = FindLongestCommonPrefixLength(field_names);
  if (lcp >= kMinCommonNamePrefixLength)
    MaybeRemovePrefix(field_names, lcp);
}

}  // namespace autofill
