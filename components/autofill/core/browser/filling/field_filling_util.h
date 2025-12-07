// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FIELD_FILLING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FIELD_FILLING_UTIL_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "components/autofill/core/browser/data_quality/addresses/address_normalizer.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// This file contains functions that are generically usefull for filling or
// functions that are used by multiple filling subdirectories.

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

// Returns an obfuscated version of `value`.
std::u16string GetObfuscatedValue(const std::u16string& value);

// Gets the country value to fill in a select control.
// Returns an empty string if no value for filling was found.
std::u16string GetCountrySelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill = nullptr);

// Returns appropriate state value that matches `field`.
// The canonical state is checked if it fits in the field and at last the
// abbreviations are tried. Does not return a state if neither |state_value| nor
// the canonical state name nor its abbreviation fit into the field.
std::u16string GetStateTextForInput(const std::u16string& state_value,
                                    const std::string& country_code,
                                    uint64_t field_max_length,
                                    std::string* failure_to_fill);

// Gets the state value to fill in a select control.
// Returns an empty string if no value for filling was found.
std::u16string GetStateSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FIELD_FILLING_UTIL_H_
