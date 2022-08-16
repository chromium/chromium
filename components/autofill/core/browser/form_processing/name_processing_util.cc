// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/name_processing_util.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
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
// Minimum required number of available fields for trying to remove affixes.
constexpr int kCommonNameAffixRemovalFieldNumberThreshold = 3;
// Minimum required length for affixes common to all field names.
constexpr int kMinCommonNameAffixLength = 3;
// Minimum required length for prefixes common to a subset of the field names.
constexpr int kMinCommonNameLongPrefixLength = 16;
// Regular expression for checking if |parseable_name| is valid after stripping
// affixes.
constexpr char16_t kParseableNameValidationRe[] = u"\\D";

using NamePieces = std::vector<base::StringPiece16>;
using OptionalNamePieces = absl::optional<NamePieces>;

// Returns the length of the longest common prefix.
// If |findCommonSuffix| is set, the length of the longest common suffix is
// returned.
size_t FindLongestCommonAffixLength(const NamePieces& strings,
                                    bool findCommonSuffix) {
  if (strings.empty())
    return 0;

  // Go through each character of the first string until there is a mismatch at
  // the same position in any other string. Adapted from http://goo.gl/YGukMM.
  for (size_t affix_len = 0; affix_len < strings[0].size(); affix_len++) {
    size_t base_string_index =
        findCommonSuffix ? strings[0].size() - affix_len - 1 : affix_len;
    for (size_t i = 1; i < strings.size(); i++) {
      size_t compared_string_index =
          findCommonSuffix ? strings[i].size() - affix_len - 1 : affix_len;
      if (affix_len >= strings[i].size() ||
          strings[i][compared_string_index] != strings[0][base_string_index]) {
        // Mismatch found.
        return affix_len;
      }
    }
  }
  return strings[0].size();
}

// Find the longest prefix in |strings| when only considering entries with a
// |minimal_length|.
size_t FindLongestCommonPrefixLengthInStringsWithMinimalLength(
    const NamePieces& strings,
    size_t minimal_length) {
  if (strings.empty())
    return 0;

  NamePieces filtered_strings;

  // Any strings less than |minimal_length| are not considered when processing
  // for a common prefix.
  std::copy_if(
      strings.begin(), strings.end(), std::back_inserter(filtered_strings),
      [&](base::StringPiece16 s) { return s.length() >= minimal_length; });

  if (filtered_strings.empty())
    return 0;

  return FindLongestCommonAffixLength(filtered_strings, false);
}

// Returns true if |parseable_name| is a valid parseable_name. Current criterion
// is the |kParseableNameValidationRe| regex.
bool IsValidParseableName(const base::StringPiece16 parseable_name) {
  return MatchesRegex<kParseableNameValidationRe>(parseable_name);
}

// Tries to strip |offset_left| and |offset_right| from entriees in
// |field_names| and checks if the resulting names are still valid parseable
// names. If not possible, return |absl::nullopt|.
OptionalNamePieces GetStrippedParseableNamesIfValid(
    const NamePieces& field_names,
    size_t offset_left,
    size_t offset_right,
    size_t minimal_string_length_to_strip) {
  NamePieces stripped_names;
  stripped_names.reserve(field_names.size());

  for (auto& parseable_name : field_names) {
    // This check allows to only strip affixes from long enough strings.
    stripped_names.push_back(
        parseable_name.size() > offset_right + offset_left &&
                parseable_name.size() >= minimal_string_length_to_strip
            ? parseable_name.substr(offset_left, parseable_name.size() -
                                                     offset_right - offset_left)
            : parseable_name);

    if (!IsValidParseableName(stripped_names.back()))
      return absl::nullopt;
  }

  return absl::make_optional(stripped_names);
}

// Tries to remove common affixes from |field_names| and returns the result. If
// neither a common affix exists, or if one or more of the resulting strings is
// not a valid parseable name, |absl::nullopt| is returned.
// The number of names in |field_names| must exceed
// |kCommonNameAffixRemovalFieldNumberThreshold| in order to make the affix
// removal possible. Also, the length of an affix must exceed
// |kMinCommonNameAffixLength| to be removed.
OptionalNamePieces RemoveCommonAffixesIfPossible(
    const NamePieces& field_names) {
  // Updates the field name parsed by heuristics if several criteria are met.
  // Several fields must be present in the form.
  if (field_names.size() < kCommonNameAffixRemovalFieldNumberThreshold)
    return absl::nullopt;

  size_t longest_prefix_length =
      FindLongestCommonAffixLength(field_names, false);
  size_t longest_suffix_length =
      FindLongestCommonAffixLength(field_names, true);

  // Don't remove the common affix if it's not long enough.
  if (longest_prefix_length < kMinCommonNameAffixLength)
    longest_prefix_length = 0;

  if (longest_suffix_length < kMinCommonNameAffixLength)
    longest_suffix_length = 0;

  // If neither a common prefix of suffix was found return false.
  if (longest_prefix_length == 0 && longest_suffix_length == 0) {
    return absl::nullopt;
  }

  // Otherwise try to reduce the names.
  return GetStrippedParseableNamesIfValid(field_names, longest_prefix_length,
                                          longest_suffix_length,
                                          /*minimal_string_length_to_strip=*/0);
}

// Tries to remove common prefixes from |field_names| and returns the result. If
// neither a common prefix exists, or if one or more of the resulting strings is
// not a valid parseable name, |absl::nullopt| is returned.
// The number of names in |field_names| must exceed
// |kCommonNamePrefixRemovalFieldThreshold| in order to make the prefix
// removal possible. Also, the length of a prefix must exceed
// |kMinCommonNamePrefixLength| to be removed.
OptionalNamePieces RemoveCommonPrefixIfPossible(const NamePieces& field_names) {
  // Updates the field name parsed by heuristics if several criteria are met.
  // Several fields must be present in the form.
  if (field_names.size() < kCommonNamePrefixRemovalFieldThreshold)
    return absl::nullopt;

  size_t longest_prefix_length =
      FindLongestCommonAffixLength(field_names, false);

  // Don't remove the common affix if it's not long enough.
  if (longest_prefix_length < kMinCommonNamePrefixLength)
    return absl::nullopt;

  // Otherwise try to reduce the names.
  return GetStrippedParseableNamesIfValid(
      field_names, longest_prefix_length, /*offset_right=*/0,
      /*minimal_string_length_to_strip=*/kMinCommonNamePrefixLength);
}

// If possible, returns field names with a removed common prefix that is common
// to the subset of names in |field_names| with a minimal length of
// |kMinCommonNameLongPrefixLength|.
// The number of names in |field_names| must exceed
// |kCommonNamePrefixRemovalFieldThreshold| in order to make the prefix
// removal possible. Also, the length of a prefix must exceed
// |kMinCommonNameLongPrefixLength| to be removed.
OptionalNamePieces RemoveCommonPrefixForNamesWithMinimalLengthIfPossible(
    const NamePieces& field_names) {
  // Update the field name parsed by heuristics if several criteria are met.
  // Several fields must be present in the form.
  if (field_names.size() < kCommonNamePrefixRemovalFieldThreshold)
    return absl::nullopt;

  const size_t longest_prefix =
      FindLongestCommonPrefixLengthInStringsWithMinimalLength(
          field_names, kMinCommonNameLongPrefixLength);
  if (longest_prefix < kMinCommonNameLongPrefixLength) {
    return absl::nullopt;
  }

  return GetStrippedParseableNamesIfValid(field_names, longest_prefix, 0,
                                          kMinCommonNameLongPrefixLength);
}

std::vector<std::u16string> GetParseableNames(const NamePieces& field_names) {
  OptionalNamePieces parseable_names = absl::nullopt;

  std::vector<std::u16string> result;
  result.reserve(field_names.size());

  // If the feature is enabled, try to remove a common affix. If this is not
  // possible try to remove lengthy prefixes that may be missing in short names.
  if (base::FeatureList::IsEnabled(features::kAutofillLabelAffixRemoval)) {
    // Try to remove both common suffixes and prefixes that are common to all
    // fields.
    parseable_names = RemoveCommonAffixesIfPossible(field_names);
    if (!parseable_names.has_value()) {
      // Remove prefix common to string that have a minimum length given by
      // |kMinCommonNamePrefixLength|.
      parseable_names =
          RemoveCommonPrefixForNamesWithMinimalLengthIfPossible(field_names);
    }

  } else {
    parseable_names = RemoveCommonPrefixIfPossible(field_names);
  }

  // If there are no parseable names after affix removal, return the original
  // field names.
  base::ranges::transform(
      parseable_names.has_value() ? parseable_names.value() : field_names,
      std::back_inserter(result), [](auto& s) { return std::u16string(s); });

  return result;
}

}  // namespace autofill
