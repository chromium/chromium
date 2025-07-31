// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/i18n/char_iterator.h"
#include "base/i18n/unicodestring.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_model/transliterator.h"
#include "components/autofill/core/browser/data_quality/addresses/address_normalizer.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

namespace {

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

// If `country_code` is set to "JP" and Katakana characters are present in the
// field label, the `value` will be transliterated from Hiragana to
// Katakana. For other countries/characters nothing will change.
std::u16string GetAlternativeNameForInput(
    const std::u16string& value,
    const AddressCountryCode& country_code,
    const FormFieldData& field_data) {
  if (country_code != AddressCountryCode("JP") ||
      !base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return value;
  }
  bool requires_conversion = data_util::HasKatakanaCharacter(field_data.label());
  // TODO(crbug.com/359768803): Remove this metric once the feature is launched.
  base::UmaHistogramBoolean(
      "Autofill.Filling.DidAlternativeNameFieldRequireConversion",
      requires_conversion);
  return requires_conversion ? TransliterateAlternativeName(
                                   value, /*inverse_transliteration=*/true)
                             : value;
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
      std::ranges::find_if(field_options, value_or_content_matches);

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
      auto selected_option = std::ranges::find_if(
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
                                          const AutofillType& autofill_type,
                                          const FormFieldData& field_data,
                                          std::string* failure_to_fill) {
  const std::u16string value = profile.GetInfo(autofill_type, app_locale);
  if (value.empty()) {
    return {};
  }
  const FieldType field_type = autofill_type.GetAddressType();
  if (GroupTypeOfFieldType(field_type) == FieldTypeGroup::kPhone) {
    return GetPhoneNumberValueForInput(
        field_data.max_length(), value,
        profile.GetInfo(PHONE_HOME_CITY_AND_NUMBER, app_locale));
  }
  if (field_type == ADDRESS_HOME_STREET_ADDRESS) {
    return GetStreetAddressForInput(value, profile.language_code(),
                                    field_data.form_control_type());
  }
  if (field_type == ADDRESS_HOME_STATE) {
    return GetStateTextForInput(
        value, data_util::GetCountryCodeWithFallback(profile, app_locale),
        field_data.max_length(), failure_to_fill);
  }
  if (IsAlternativeNameType(field_type)) {
    return GetAlternativeNameForInput(value, profile.GetAddressCountryCode(),
                                      field_data);
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
    const AutofillType& autofill_type,
    const FormFieldData& field_data,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  const FieldType field_type = autofill_type.GetAddressType();
  CHECK_NE(field_type, UNKNOWN_TYPE);
  std::u16string value = GetValueForProfileForInput(
      profile, app_locale, autofill_type, field_data, failure_to_fill);

  if (field_data.IsSelectElement() && !value.empty()) {
    value = GetValueForProfileSelectControl(
        profile, value, app_locale, field_data.options(), field_type,
        address_normalizer, failure_to_fill);
  }

  return {value, field_type};
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
