// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SELECT_CONTROL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SELECT_CONTROL_UTIL_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Returns the index of the shortest entry in the given select field of which
// |value| is a substring. Returns -1 if no such entry exists.
int FindShortestSubstringMatchInSelect(
    const std::u16string& value,
    bool ignore_whitespace,
    base::span<const SelectOption> field_options);

// Searches for an exact match for `value` in `field_options` and returns it
// if found, or std::nullopt otherwise. Optionally, the caller may pass
// `best_match_index` which will be set to the index of the best match.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill,
    size_t* best_match_index = nullptr);

// Like GetSelectControlValue, but searches within the field values and options
// for `value`. For example, "NC - North Carolina" would match "north carolina".
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetSelectControlValueSubstringMatch(
    const std::u16string& value,
    bool ignore_whitespace,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill);

// Like GetSelectControlValue, but searches within the field values and options
// for `value`. First it tokenizes the options, then tries to match against
// tokens. For example, "NC - North Carolina" would match "nc" but not "ca".
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetSelectControlValueTokenMatch(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill);

// Gets the numeric `value` to fill into `field`.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetNumericSelectControlValue(
    int value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SELECT_CONTROL_UTIL_H_
