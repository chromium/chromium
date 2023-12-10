// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filling_address_util.h"

#include <optional>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/select_control_util.h"
#include "components/autofill/core/common/autofill_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"

namespace autofill {

namespace {

// Helper method to normalize the `admin_area` for the given `country_code`.
// The value in `admin_area` will be overwritten.
bool NormalizeAdminAreaForCountryCode(std::u16string* admin_area,
                                      const std::string& country_code,
                                      AddressNormalizer* address_normalizer) {
  DCHECK(address_normalizer);
  DCHECK(admin_area);
  if (admin_area->empty() || country_code.empty()) {
    return false;
  }

  AutofillProfile tmp_profile((AddressCountryCode(country_code)));
  tmp_profile.SetRawInfo(ADDRESS_HOME_STATE, *admin_area);
  if (!address_normalizer->NormalizeAddressSync(&tmp_profile)) {
    return false;
  }

  *admin_area = tmp_profile.GetRawInfo(ADDRESS_HOME_STATE);
  return true;
}

// Will normalize `value` and the options in `field` with `address_normalizer`
// (which should not be null), and return whether the fill was successful.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetNormalizedStateSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  DCHECK(address_normalizer);
  // We attempt to normalize a copy of the field value. If normalization was not
  // successful, it means the rules were probably not loaded. Give up. Note that
  // the normalizer will fetch the rule for next time it's called.
  // TODO(crbug.com/788417): We should probably sanitize |value| before
  // normalizing.
  std::u16string field_value = value;
  if (!NormalizeAdminAreaForCountryCode(&field_value, country_code,
                                        address_normalizer)) {
    if (failure_to_fill) {
      *failure_to_fill += "Could not normalize admin area for country code. ";
    }
    return std::nullopt;
  }

  // If successful, try filling the normalized value with the existing field
  // |options|.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(field_value, field_options, failure_to_fill)) {
    return select_control_value;
  }

  // Normalize `field_options` using a copy.
  // TODO(crbug.com/788417): We should probably sanitize the values in
  // `field_options_copy` before normalizing.
  bool normalized = false;
  std::vector<SelectOption> field_options_copy(field_options.begin(),
                                               field_options.end());
  for (SelectOption& option : field_options_copy) {
    normalized |= NormalizeAdminAreaForCountryCode(&option.value, country_code,
                                                   address_normalizer);
    normalized |= NormalizeAdminAreaForCountryCode(
        &option.content, country_code, address_normalizer);
  }

  // Try filling the normalized value with the existing `field_options_copy`.
  size_t best_match_index = 0;
  if (normalized && GetSelectControlValue(field_value, field_options_copy,
                                          failure_to_fill, &best_match_index)) {
    // `best_match_index` now points to the option in `field->options`
    // that corresponds to our best match.
    return field_options[best_match_index].value;
  }
  if (failure_to_fill) {
    *failure_to_fill += "Could not set normalized state in control element. ";
  }
  return std::nullopt;
}

// Gets the state value to fill in a select control.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetStateSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  std::vector<std::u16string> abbreviations;
  std::vector<std::u16string> full_names;

  // Fetch the corresponding entry from AlternativeStateNameMap.
  absl::optional<StateEntry> state_entry =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode(country_code),
          AlternativeStateNameMap::StateName(value));

  // State abbreviations will be empty for non-US countries.
  if (state_entry) {
    for (const std::string& abbreviation : state_entry->abbreviations()) {
      if (!abbreviation.empty()) {
        abbreviations.push_back(base::UTF8ToUTF16(abbreviation));
      }
    }
    if (state_entry->has_canonical_name()) {
      full_names.push_back(base::UTF8ToUTF16(state_entry->canonical_name()));
    }
    for (const std::string& alternative_name :
         state_entry->alternative_names()) {
      full_names.push_back(base::UTF8ToUTF16(alternative_name));
    }
  } else {
    if (value.size() > 2) {
      full_names.push_back(value);
    } else if (!value.empty()) {
      abbreviations.push_back(value);
    }
  }

  std::u16string state_name;
  std::u16string state_abbreviation;
  state_names::GetNameAndAbbreviation(value, &state_name, &state_abbreviation);

  full_names.push_back(std::move(state_name));
  if (!state_abbreviation.empty()) {
    abbreviations.push_back(std::move(state_abbreviation));
  }

  // Remove `abbreviations` from the `full_names` as a precautionary measure in
  // case the `AlternativeStateNameMap` contains bad data.
  base::ranges::sort(abbreviations);
  std::erase_if(full_names, [&](const std::u16string& full_name) {
    return full_name.empty() ||
           base::ranges::binary_search(abbreviations, full_name);
  });

  // Try an exact match of the abbreviation first.
  for (const std::u16string& abbreviation : abbreviations) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValue(abbreviation, field_options,
                                  failure_to_fill)) {
      return select_control_value;
    }
  }

  // Try an exact match of the full name.
  for (const std::u16string& full : full_names) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValue(full, field_options, failure_to_fill)) {
      return select_control_value;
    }
  }

  // Try an inexact match of the full name.
  for (const std::u16string& full : full_names) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValueSubstringMatch(
                full, /*ignore_whitespace=*/false, field_options,
                failure_to_fill)) {
      return select_control_value;
    }
  }

  // Try an inexact match of the abbreviation name.
  for (const std::u16string& abbreviation : abbreviations) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValueTokenMatch(abbreviation, field_options,
                                            failure_to_fill)) {
      return select_control_value;
    }
  }

  if (!address_normalizer) {
    if (failure_to_fill) {
      *failure_to_fill += "Could not fill state in select control element. ";
    }
    return std::nullopt;
  }

  // Try to match a normalized `value` of the state and the `field_options`.
  return GetNormalizedStateSelectControlValue(
      value, field_options, country_code, address_normalizer, failure_to_fill);
}

// Gets the country value to fill in a select control.
// A nullopt value means that no value for filling was found.
std::optional<std::u16string> GetCountrySelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  // Search for exact matches.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(value, field_options, failure_to_fill)) {
    return select_control_value;
  }
  std::string country_code = CountryNames::GetInstance()->GetCountryCode(value);
  if (country_code.empty()) {
    if (failure_to_fill) {
      *failure_to_fill += "Cannot fill empty country code. ";
    }
    return std::nullopt;
  }

  for (const SelectOption& option : field_options) {
    // Canonicalize each <option> value to a country code, and compare to the
    // target country code.
    if (country_code ==
            CountryNames::GetInstance()->GetCountryCode(option.value) ||
        country_code ==
            CountryNames::GetInstance()->GetCountryCode(option.content)) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find country to fill in select control element. ";
  }
  return std::nullopt;
}

// Returns appropriate street address for `field`. Translates newlines into
// equivalent separators when necessary, i.e. when filling a single-line field.
// The separators depend on `address_language_code`.
std::u16string GetStreetAddressForInput(
    const std::u16string& address_value,
    const std::string& address_language_code,
    FormControlType form_control_type) {
  if (form_control_type == FormControlType::kTextArea) {
    return address_value;
  }
  ::i18n::addressinput::AddressData address_data;
  address_data.language_code = address_language_code;
  address_data.address_line =
      base::SplitString(base::UTF16ToUTF8(address_value), "\n",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string line;
  ::i18n::addressinput::GetStreetAddressLinesAsSingleLine(address_data, &line);
  return base::UTF8ToUTF16(line);
}

// Returns appropriate state value that matches `field`.
// The canonical state is checked if it fits in the field and at last the
// abbreviations are tried. Does not return a state if neither |state_value| nor
// the canonical state name nor its abbreviation fit into the field.
std::optional<std::u16string> GetStateTextForInput(
    const std::u16string& state_value,
    const std::string& country_code,
    uint64_t field_max_length,
    std::string* failure_to_fill) {
  if (field_max_length == 0 || field_max_length >= state_value.size()) {
    // Return the state value directly.
    return state_value;
  }
  absl::optional<StateEntry> state =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode(country_code),
          AlternativeStateNameMap::StateName(state_value));
  if (state) {
    // Return the canonical state name if possible.
    if (state->has_canonical_name() && !state->canonical_name().empty() &&
        field_max_length >= state->canonical_name().size()) {
      return base::UTF8ToUTF16(state->canonical_name());
    }
    // Return the abbreviation if possible.
    for (const auto& abbreviation : state->abbreviations()) {
      if (!abbreviation.empty() && field_max_length >= abbreviation.size()) {
        return base::i18n::ToUpper(base::UTF8ToUTF16(abbreviation));
      }
    }
  }
  // Return with the state abbreviation.
  std::u16string abbreviation;
  state_names::GetNameAndAbbreviation(state_value, nullptr, &abbreviation);
  if (!abbreviation.empty() && field_max_length >= abbreviation.size()) {
    return base::i18n::ToUpper(abbreviation);
  }
  if (failure_to_fill) {
    *failure_to_fill += "Could not fit raw state nor abbreviation. ";
  }
  return std::nullopt;
}

// Finds the best suitable option in the `field` that corresponds to the
// `country_code`.
// If the exact match is not found, extracts the digits (ignoring leading '00'
// or '+') from each option and compares them with the `country_code`.
std::optional<std::u16string> GetPhoneCountryCodeSelectControlForInput(
    const std::u16string& country_code,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  if (country_code.empty()) {
    return std::nullopt;
  }
  // Find the option that exactly matches the |country_code|.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(country_code, field_options, failure_to_fill)) {
    return *select_control_value;
  }
  for (const SelectOption& option : field_options) {
    std::u16string cc_candidate_in_value =
        data_util::FindPossiblePhoneCountryCode(RemoveWhitespace(option.value));
    std::u16string cc_candidate_in_content =
        data_util::FindPossiblePhoneCountryCode(
            RemoveWhitespace(option.content));
    if (cc_candidate_in_value == country_code ||
        cc_candidate_in_content == country_code) {
      return option.value;
    }
  }
  if (failure_to_fill) {
    *failure_to_fill += "Could not match to formatted country code options. ";
  }
  return std::nullopt;
}

// Returns the appropriate `profile` value based on `field_type` to fill
// into the input `field`.
std::optional<std::u16string> GetValueForProfileForInput(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const AutofillType& field_type,
    const FormFieldData& field_data,
    std::string* failure_to_fill) {
  const std::u16string value = profile.GetInfo(field_type, app_locale);
  if (value.empty()) {
    return std::nullopt;
  }
  if (field_type.group() == FieldTypeGroup::kPhone) {
    return GetPhoneNumberValueForInput(
        field_data.max_length, value,
        profile.GetInfo(PHONE_HOME_CITY_AND_NUMBER, app_locale));
  }
  if (field_type.GetStorableType() == ADDRESS_HOME_STREET_ADDRESS) {
    return GetStreetAddressForInput(value, profile.language_code(),
                                    field_data.form_control_type);
  }
  if (field_type.GetStorableType() == ADDRESS_HOME_STATE) {
    return GetStateTextForInput(
        value, data_util::GetCountryCodeWithFallback(profile, app_locale),
        field_data.max_length, failure_to_fill);
  }
  return std::move(value);
}

std::optional<std::u16string> GetValueForProfileSelectControl(
    const AutofillProfile& profile,
    const std::u16string& value,
    const std::string& app_locale,
    base::span<const SelectOption> field_options,
    ServerFieldType field_type,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  switch (field_type) {
    case ADDRESS_HOME_COUNTRY:
      return GetCountrySelectControlValue(value, field_options,
                                          failure_to_fill);
    case ADDRESS_HOME_STATE:
      return GetStateSelectControlValue(
          value, field_options,
          data_util::GetCountryCodeWithFallback(profile, app_locale),
          address_normalizer, failure_to_fill);
    case PHONE_HOME_COUNTRY_CODE:
      return GetPhoneCountryCodeSelectControlForInput(value, field_options,
                                                      failure_to_fill);
    default:
      return GetSelectControlValue(value, field_options, failure_to_fill);
  }
}

}  // namespace

std::optional<std::u16string> GetValueForProfile(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const AutofillType& field_type,
    const FormFieldData& field_data,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  std::optional<std::u16string> value = GetValueForProfileForInput(
      profile, app_locale, field_type, field_data, failure_to_fill);

  return value && field_data.IsSelectOrSelectListElement()
             ? GetValueForProfileSelectControl(
                   profile, *value, app_locale, field_data.options,
                   field_type.GetStorableType(), address_normalizer,
                   failure_to_fill)
             : value;
}

std::u16string GetPhoneNumberValueForInput(
    uint64_t field_max_length,
    const std::u16string& number,
    const std::u16string& city_and_number) {
  // If the complete `number` fits into the field return it as is.
  // `field_max_length == 0` means that there's no size limit.
  if (field_max_length == 0 || field_max_length >= number.length()) {
    return number;
  }

  // Try after removing the country code, if `number` exceeds the maximum size
  // of the field.
  if (city_and_number.length() <= field_max_length) {
    return city_and_number;
  }

  // If `number` exceeds the maximum size of the field, cut the first part to
  // provide a valid number for the field. For example, the number 15142365264
  // with a field with a max length of 10 would return 5142365264, thus
  // filling in the last `field_data.max_length` characters from the `number`.
  return number.substr(number.length() - field_max_length, field_max_length);
}

}  // namespace autofill
