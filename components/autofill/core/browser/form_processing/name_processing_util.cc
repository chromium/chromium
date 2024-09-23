// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
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

// Returns true if `parseable_name` is a valid parseable_name. To be considered
// valid, the string cannot be empty or consist of digits only.
// This condition prevents the logic from simplifying strings like
// "address-line-1", "address-line-2" to "1", "2".
bool IsValidParseableName(std::u16string_view parseable_name) {
  static constexpr char16_t kRegex[] = u"\\D";
  return MatchesRegex<kRegex>(parseable_name);
}

// Tries to remove an affix of length `len` from all `strings`. The removal
// fails if one of the resulting strings is not `IsValidParseableName()`.
// Assumes that all strings are at least `len` long.
void MaybeRemoveAffix(base::span<std::u16string_view> strings,
                      size_t len,
                      bool prefix) {
  DCHECK(std::ranges::all_of(
      strings, [&](std::u16string_view s) { return s.size() >= len; }));
  auto RemoveAffix = [&](std::u16string_view s) {
    if (prefix) {
      s.remove_prefix(len);
    } else {
      s.remove_suffix(len);
    }
    return s;
  };
  if (std::ranges::all_of(strings, [&](std::u16string_view s) {
        return IsValidParseableName(RemoveAffix(s));
      })) {
    base::ranges::transform(strings, strings.begin(), RemoveAffix);
  }
}

}  // namespace

size_t FindLongestCommonAffixLength(base::span<std::u16string_view> strings,
                                    bool prefix) {
  if (strings.empty())
    return 0;

  size_t affix_len = 0;
  auto AgreeOnNextChar = [&](std::u16string_view other) {
    if (affix_len >= other.size())
      return false;
    return prefix ? strings[0][affix_len] == other[affix_len]
                  : strings[0].rbegin()[affix_len] == other.rbegin()[affix_len];
  };
  while (std::ranges::all_of(strings, AgreeOnNextChar)) {
    ++affix_len;
  }
  return affix_len;
}

void ComputeParseableNames(base::span<std::u16string_view> field_names) {
  if (field_names.size() < kCommonNamePrefixRemovalFieldThreshold)
    return;
  size_t lcp = FindLongestCommonAffixLength(field_names, /*prefix=*/true);
  if (lcp >= kMinCommonNamePrefixLength)
    MaybeRemoveAffix(field_names, lcp, /*prefix=*/true);
}

}  // namespace autofill
