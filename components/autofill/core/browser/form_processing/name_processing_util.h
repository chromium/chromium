// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/browser/form_structure.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

#ifdef UNIT_TEST
size_t FindLongestCommonAffixLength(
    const std::vector<base::StringPiece16>& strings,
    bool findCommonSuffix);

bool IsValidParseableName(const base::StringPiece16 parseable_name);

absl::optional<std::vector<base::StringPiece16>> RemoveCommonAffixesIfPossible(
    const std::vector<base::StringPiece16>& field_names);

size_t FindLongestCommonPrefixLengthInStringsWithMinimalLength(
    const std::vector<base::StringPiece16>& strings,
    size_t minimal_length);

absl::optional<std::vector<base::StringPiece16>>
GetStrippedParseableNamesIfValid(
    const std::vector<base::StringPiece16>& field_names,
    size_t offset_left,
    size_t offset_right,
    size_t minimal_string_length_to_strip);

absl::optional<std::vector<base::StringPiece16>> RemoveCommonPrefixIfPossible(
    const std::vector<base::StringPiece16>& field_names);

absl::optional<std::vector<base::StringPiece16>>
RemoveCommonPrefixForNamesWithMinimalLengthIfPossible(
    const std::vector<base::StringPiece16>& field_names);
#endif

// Determines and returns the parseable names for |field_names|.
// With the |kAutofillLabelAffixRemoval| feature enabled, first it is tried to
// remove a common affix from all names in |field_names|. If this is not
// possible, it is attempted to remove long prefixes from a subset of names in
// |field_names| which exceed a given length. If the
// |kAutofillLabelAffixRemoval| is disabled, a prefix removal is attempted. In
// any case, if a affix/prefix removal is not possible, the original names in
// |field_names| are returned.
//
// Beware, this function works on string pieces and therefore, it should not be
// called with temporary objects. Also, the underlying strings should not be
// modified before the last usage of the result.
std::vector<std::u16string> GetParseableNames(
    const std::vector<base::StringPiece16>& field_names);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_
