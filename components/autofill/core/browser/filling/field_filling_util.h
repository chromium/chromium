// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FIELD_FILLING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FIELD_FILLING_UTIL_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

class AddressNormalizer;

struct FillingValueAndType {
  FillingValueAndType();
  FillingValueAndType(std::u16string value, FieldType filling_type);
  FillingValueAndType(std::u16string value,
                      std::optional<std::u16string> select_text,
                      FieldType filling_type);
  FillingValueAndType(const FillingValueAndType&);
  FillingValueAndType(FillingValueAndType&&);
  FillingValueAndType& operator=(const FillingValueAndType&);
  FillingValueAndType& operator=(FillingValueAndType&&);
  ~FillingValueAndType();

  std::u16string value = u"";
  // `select_text` is only specified for <select> fields to disambiguate
  // multiple options that have the same `SelectOption::value`.
  std::optional<std::u16string> select_text = std::nullopt;
  FieldType filling_type = NO_SERVER_DATA;
};

// This file contains functions that are generically usefull for filling or
// functions that are used by multiple filling subdirectories.

// Searches for an exact match for `value` in `field_options` and returns the
// matching option if found, or std::nullopt otherwise. Optionally, the caller
// may pass `best_match_index` which will be set to the index of the best match.
// A nullopt value means that no value for filling was found.
std::optional<SelectOption> GetSelectControlOption(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill,
    size_t* best_match_index = nullptr);

// Like GetSelectControlOption, but searches within the field values and options
// for `value`. For example, "NC - North Carolina" would match "north carolina".
// A nullopt value means that no value for filling was found.
std::optional<SelectOption> GetSelectControlOptionSubstringMatch(
    const std::u16string& value,
    bool ignore_whitespace,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill);

// Gets the option containing the numeric `value` among `field_options`.
// A nullopt value means that no value for filling was found.
std::optional<SelectOption> GetNumericSelectControlOption(
    int value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill);

// Returns an obfuscated version of `value`.
// `visible_suffix_length` defines how many of the last n characters should
// not be obfuscated.
// TODO(crbug.com/394011769): Remove visible_suffix_length once
// kAutofillAiWalletPrivatePasses is rolled out and replace it with an
// 'obfuscate_all' bool.
std::u16string GetObfuscatedValue(const std::u16string& value,
                                  size_t visible_suffix_length = 0);

// Gets the country option to fill in a select control.
// Returns an empty string if no value for filling was found.
std::optional<SelectOption> GetCountrySelectControlOption(
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

// Gets the state option to fill in a select control.
// Returns an empty string if no value for filling was found.
std::optional<SelectOption> GetStateSelectControlOption(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FIELD_FILLING_UTIL_H_
