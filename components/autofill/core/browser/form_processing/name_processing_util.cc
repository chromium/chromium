// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

// Only removing common name prefixes if we have a minimum number of fields and
// a minimum prefix length. These values are chosen to avoid cases such as two
// fields with "address1" and "address2" and be effective against web frameworks
// which prepend prefixes such as "ctl01$ctl00$MainContentRegion$" on all
// fields.
constexpr int kCommonNamePrefixRemovalFieldThreshold = 3;
// Minimum required length for prefixes common to a subset of the field names.
constexpr int kMinCommonNamePrefixLength = 16;

using NamePieces = std::vector<base::StringPiece16>;

// Returns the length of the longest common prefix of the `strings`. The runtime
// is O(strings.size() * length-of-longest-common-prefix).
size_t FindLongestCommonPrefixLength(const NamePieces& strings) {
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

// Returns true if `parseable_name` is a valid parseable_name. To be considered
// valid, the string cannot be empty or consist of digits only.
// This condition prevents the logic from simplifying strings like
// "address-line-1", "address-line-2" to "1", "2".
bool IsValidParseableName(base::StringPiece16 parseable_name) {
  static constexpr char16_t kRegex[] = u"\\D";
  return MatchesRegex<kRegex>(parseable_name);
}

// Tries to remove common prefixes from `field_names` and returns the result. If
// neither a common prefix exists, or if one or more of the resulting strings is
// not a valid parseable name, `absl::nullopt` is returned.
// The number of names in `field_names` must exceed
// `kCommonNamePrefixRemovalFieldThreshold` in order to make the prefix
// removal possible. Also, the length of a prefix must exceed
// `kMinCommonNamePrefixLength` to be removed.
absl::optional<NamePieces> RemoveCommonPrefixIfPossible(
    const NamePieces& field_names) {
  if (field_names.size() < kCommonNamePrefixRemovalFieldThreshold)
    return absl::nullopt;

  size_t longest_prefix_length = FindLongestCommonPrefixLength(field_names);
  if (longest_prefix_length < kMinCommonNamePrefixLength)
    return absl::nullopt;

  NamePieces stripped_names;
  stripped_names.reserve(field_names.size());
  for (auto& parseable_name : field_names) {
    stripped_names.push_back(parseable_name.substr(longest_prefix_length));
    if (!IsValidParseableName(stripped_names.back()))
      return absl::nullopt;
  }
  return stripped_names;
}

NamePieces GetParseableNamesAsStringPiece(const NamePieces* field_names) {
  // TODO(crbug.com/1355264): Revise the `AutofillLabelAffixRemoval` feature.
  return RemoveCommonPrefixIfPossible(*field_names).value_or(*field_names);
}

}  // namespace autofill
