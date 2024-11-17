// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filling_address_util.h"

#include <optional>

#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/select_control_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

namespace {

// Helper method to normalize the `admin_area` for the given `country_code`.
// The value in `admin_area` will be overwritten.
bool NormalizeAdminAreaForCountryCode(std::u16string& admin_area,
                                      const std::string& country_code,
                                      AddressNormalizer& address_normalizer) {
  if (admin_area.empty() || country_code.empty()) {
    return false;
  }

  AutofillProfile tmp_profile((AddressCountryCode(country_code)));
  tmp_profile.SetRawInfo(ADDRESS_HOME_STATE, admin_area);
  if (!address_normalizer.NormalizeAddressSync(&tmp_profile)) {
    return false;
  }

  admin_area = tmp_profile.GetRawInfo(ADDRESS_HOME_STATE);
  return true;
}

// Returns the SelectOption::value of `field_options` that best matches the
// normalized `value`. Returns an empty string if no match is found.
// Normalization is relative to the `country_code` and to `address_normalizer`.
std::u16string GetNormalizedStateSelectControlValue(
    std::u16string value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer& address_normalizer,
    std::string* failure_to_fill) {
  // We attempt to normalize `value`. If normalization was not successful, it
  // means the rules were probably not loaded. Give up. Note that the normalizer
  // will fetch the rule next time it's called.
  // TODO(crbug.com/40551524): We should probably sanitize |value| before
  // normalizing.
  if (!NormalizeAdminAreaForCountryCode(value, country_code,
                                        address_normalizer)) {
    if (failure_to_fill) {
      *failure_to_fill += "Could not normalize admin area for country code. ";
    }
    return {};
  }

  // If successful, try filling the normalized value with the existing field
  // |options|.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(value, field_options, failure_to_fill)) {
    return *select_control_value;
  }

  // Normalize `field_options` using a copy.
  // TODO(crbug.com/40551524): We should probably sanitize the values in
  // `field_options_copy` before normalizing.
  bool normalized = false;
  std::vector<SelectOption> field_options_copy(field_options.begin(),
                                               field_options.end());
  for (SelectOption& option : field_options_copy) {
    normalized |= NormalizeAdminAreaForCountryCode(option.value, country_code,
                                                   address_normalizer);
    normalized |= NormalizeAdminAreaForCountryCode(option.text, country_code,
                                                   address_normalizer);
  }

  // Try filling the normalized value with the existing `field_options_copy`.
  size_t best_match_index = 0;
  if (normalized && GetSelectControlValue(value, field_options_copy,
                                          failure_to_fill, &best_match_index)) {
    // `best_match_index` now points to the option in `field->options`
    // that corresponds to our best match.
    return field_options[best_match_index].value;
  }
  if (failure_to_fill) {
    *failure_to_fill += "Could not set normalized state in control element. ";
  }
  return {};
}

// Gets the state value to fill in a select control.
// Returns an empty string if no value for filling was found.
std::u16string GetStateSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  std::vector<std::u16string> abbreviations;
  std::vector<std::u16string> full_names;

  // Fetch the corresponding entry from AlternativeStateNameMap.
  std::optional<StateEntry> state_entry =
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
      return *select_control_value;
    }
  }

  // Try an exact match of the full name.
  for (const std::u16string& full : full_names) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValue(full, field_options, failure_to_fill)) {
      return *select_control_value;
    }
  }

  // Try an inexact match of the full name.
  for (const std::u16string& full : full_names) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValueSubstringMatch(
                full, /*ignore_whitespace=*/false, field_options,
                failure_to_fill)) {
      return *select_control_value;
    }
  }

  // Try an inexact match of the abbreviation name.
  for (const std::u16string& abbreviation : abbreviations) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValueTokenMatch(abbreviation, field_options,
                                            failure_to_fill)) {
      return *select_control_value;
    }
  }

  if (!address_normalizer) {
    if (failure_to_fill) {
      *failure_to_fill += "Could not fill state in select control element. ";
    }
    return {};
  }

  // Try to match a normalized `value` of the state and the `field_options`.
  return GetNormalizedStateSelectControlValue(
      value, field_options, country_code, *address_normalizer, failure_to_fill);
}

// Gets the country value to fill in a select control.
// Returns an empty string if no value for filling was found.
std::u16string GetCountrySelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill = nullptr) {
  // Search for exact matches.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(value, field_options, failure_to_fill)) {
    return *select_control_value;
  }
  std::string country_code = CountryNames::GetInstance()->GetCountryCode(value);
  if (country_code.empty()) {
    if (failure_to_fill) {
      *failure_to_fill += "Cannot fill empty country code. ";
    }
    return {};
  }

  // Sometimes options contain a country name and phone country code (e.g.
  // "Germany (+49)"). This can happen if such a <select> is annotated as
  // autocomplete="tel-country-code". The following lambda strips the phone
  // country code so that the remainder ideally matches a country name.
  auto strip_phone_country_code =
      [](const std::u16string& value) -> std::u16string {
    static base::NoDestructor<std::unique_ptr<const RE2>> regex_pattern(
        std::make_unique<const RE2>("[(]?(?:00|\\+)\\s*[1-9]\\d{0,3}[)]?"));
    std::string u8string = base::UTF16ToUTF8(value);
    if (RE2::Replace(&u8string, **regex_pattern, "")) {
      return base::UTF8ToUTF16(
          base::TrimWhitespaceASCII(u8string, base::TRIM_ALL));
    }
    return value;
  };

  for (const SelectOption& option : field_options) {
    // Canonicalize each <option> value to a country code, and compare to the
    // target country code.
    if (country_code == CountryNames::GetInstance()->GetCountryCode(
                            strip_phone_country_code(option.value)) ||
        country_code == CountryNames::GetInstance()->GetCountryCode(
                            strip_phone_country_code(option.text))) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find country to fill in select control element. ";
  }
  return {};
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
std::u16string GetStateTextForInput(const std::u16string& state_value,
                                    const std::string& country_code,
                                    uint64_t field_max_length,
                                    std::string* failure_to_fill) {
  if (field_max_length == 0 || field_max_length >= state_value.size()) {
    // Return the state value directly.
    return state_value;
  }
  std::optional<StateEntry> state =
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
  return {};
}

// Finds the best suitable option in the `field` that corresponds to the
// `phone_country_code`. The strategy is:
// - If a <select> menu has an option whose value or content exactly matches the
//   phone country code (e.g. "1" for US), this is picked (e.g. <option
//   value="1">+1</option>).
// - As a fallback, the first option with text containing a phone country code
//   (ignoring leading '00' or '+') (e.g. <option value="US">USA (+1)</option>)
//   is picked The fallback broken in case there is ambiguity (e.g. there is a
//   second option
//   <option value="CA">Canada (+1)</option>)
//
// If kAutofillEnableFillingPhoneCountryCodesByAddressCountryCodes is enabled,
// Autofill will
// - pick an option whose value or content MATCHES EXACTLY the phone country
//   code (e.g. "1" for US) (old behavior as above), else
// - pick an option whose value or content CONTAINS with prefix (e.g. "+1" or
//   "001" for US) the phone country code if it's unambiguous, else
// - pick an option whose value or content MATCHES the address country code or
//   country name (after removing a phone country code like "+1") if that's
//   possible.
//   - If the options contain phone country codes ("+1", "001"), then this path
//     is only chosen if the chosen option from the parent bullet CONTAINS the
//     desired phone country code.
//   else
// - pick the FIRST option whose value or content CONTAINS the phone country
//   code with prefix (old behavior).
// TODO(crbug.com/40249216) Clean up the comment above when the feature is
// launched.
std::u16string GetPhoneCountryCodeSelectControlValue(
    const std::u16string& phone_country_code,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    std::string* failure_to_fill) {
  if (phone_country_code.empty()) {
    return {};
  }
  // Find the option that exactly matches the |phone_country_code|.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(phone_country_code, field_options,
                                failure_to_fill)) {
    return *select_control_value;
  }

  auto value_or_content_matches = [&](const SelectOption& option) {
    return data_util::FindPossiblePhoneCountryCode(option.value) ==
               phone_country_code ||
           data_util::FindPossiblePhoneCountryCode(option.text) ==
               phone_country_code;
  };
  auto first_match =
      base::ranges::find_if(field_options, value_or_content_matches);

  if (base::FeatureList::IsEnabled(
          features::
              kAutofillEnableFillingPhoneCountryCodesByAddressCountryCodes)) {
    // If a single option contained the phone country code, return that.
    if (first_match != field_options.end() &&
        std::ranges::none_of(first_match + 1, field_options.end(),
                             value_or_content_matches)) {
      return first_match->value;
    }

    // Either more than one option matched the country code (this is common for
    // +1, which is associated with Canada the USA and several other countries)
    // or none matched the country code. Try to match by address country code or
    // name.
    if (std::u16string option = GetCountrySelectControlValue(
            base::UTF8ToUTF16(country_code), field_options);
        !option.empty()) {
      // If the <option>s don't contain phone country codes, the country name
      // is the best insight we have, so we go with it.
      if (first_match == field_options.end()) {
        return option;
      }
      // If the <option>s do contain phone country codes, we pick the current
      // option only if the phone country code matches.
      auto selected_option = base::ranges::find_if(
          field_options,
          [&](const SelectOption& o) { return o.value == option; });
      if (value_or_content_matches(*selected_option)) {
        return option;
      }
    }

    // Matching by country name failed, so return the first entry containing
    // the phone country code if that exists.
  }
  if (first_match != field_options.end()) {
    return first_match->value;
  }

  if (failure_to_fill) {
    *failure_to_fill += "Could not match to formatted country code options. ";
  }
  return {};
}

// Returns the appropriate `profile` value based on `field_type` to fill
// into the input `field`.
std::u16string GetValueForProfileForInput(const AutofillProfile& profile,
                                          const std::string& app_locale,
                                          const AutofillType& field_type,
                                          const FormFieldData& field_data,
                                          std::string* failure_to_fill) {
  const std::u16string value = profile.GetInfo(field_type, app_locale);
  if (value.empty()) {
    return {};
  }
  if (field_type.group() == FieldTypeGroup::kPhone) {
    return GetPhoneNumberValueForInput(
        field_data.max_length(), value,
        profile.GetInfo(PHONE_HOME_CITY_AND_NUMBER, app_locale));
  }
  if (field_type.GetStorableType() == ADDRESS_HOME_STREET_ADDRESS) {
    return GetStreetAddressForInput(value, profile.language_code(),
                                    field_data.form_control_type());
  }
  if (field_type.GetStorableType() == ADDRESS_HOME_STATE) {
    return GetStateTextForInput(
        value, data_util::GetCountryCodeWithFallback(profile, app_locale),
        field_data.max_length(), failure_to_fill);
  }
  return value;
}

std::u16string GetValueForProfileSelectControl(
    const AutofillProfile& profile,
    const std::u16string& value,
    const std::string& app_locale,
    base::span<const SelectOption> field_options,
    FieldType field_type,
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
      return GetPhoneCountryCodeSelectControlValue(
          value, field_options,
          data_util::GetCountryCodeWithFallback(profile, app_locale),
          failure_to_fill);
    default:
      return GetSelectControlValue(value, field_options, failure_to_fill)
          .value_or(u"");
  }
}

}  // namespace

std::pair<std::u16string, FieldType> GetFillingValueAndTypeForProfile(
    const AutofillProfile& profile,
    const std::string& app_locale,
    const AutofillType& field_type,
    const FormFieldData& field_data,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  AutofillType filling_type = profile.GetFillingType(field_type);
  CHECK(IsAddressType(filling_type.GetStorableType()));
  std::u16string value = GetValueForProfileForInput(
      profile, app_locale, filling_type, field_data, failure_to_fill);

  if (field_data.IsSelectElement() && !value.empty()) {
    value = GetValueForProfileSelectControl(
        profile, value, app_locale, field_data.options(),
        filling_type.GetStorableType(), address_normalizer, failure_to_fill);
  }

  return {value, filling_type.GetStorableType()};
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
