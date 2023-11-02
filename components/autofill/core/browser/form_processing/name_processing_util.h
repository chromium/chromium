// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

#ifdef UNIT_TEST
size_t FindLongestCommonPrefixLength(
    const std::vector<base::StringPiece16>& strings);

bool IsValidParseableName(base::StringPiece16 parseable_name);

absl::optional<std::vector<base::StringPiece16>> RemoveCommonPrefixIfPossible(
    const std::vector<base::StringPiece16>& field_names);
#endif

// Determines and returns the parseable names of `field_names`, by removing
// long common prefixes. If the common prefix is too short or empty, the
// original names in `field_names` are returned.
// While this function works on a general set of strings, it is solely used for
// the purpose of "rationalizing" the names of `FormFieldData::name`. The result
// is then referred to as the "parseable name" of the field. Hence the
// terminology here.
std::vector<base::StringPiece16> GetParseableNamesAsStringPiece(
    const std::vector<base::StringPiece16>* field_names);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_
