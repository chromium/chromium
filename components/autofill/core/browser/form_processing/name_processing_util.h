// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_

#include <string_view>

#include "base/containers/span.h"

namespace autofill {

// Returns the length of the longest common affix of the `strings`. If `prefix`
// is true, the prefixes are considered, otherwise the suffixes.
// The runtime is O(strings.size() * length-of-longest-common-affix).
size_t FindLongestCommonAffixLength(base::span<std::u16string_view> strings,
                                    bool prefix);

// Removes long common prefixes from `field_names`. If the common prefix is too
// short or empty, `field_names` are left unmodified.
// While this function works on a general set of strings, it is solely used for
// the purpose of "rationalizing" the names of `FormFieldData::name`. The result
// is then referred to as the "parseable name" of the field. Hence the
// terminology here.
void ComputeParseableNames(base::span<std::u16string_view> field_names);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_NAME_PROCESSING_UTIL_H_
