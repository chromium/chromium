// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filler.h"

#include <stdint.h>
#include <vector>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/proto/states.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;
using base::StringToInt;

namespace autofill {

namespace {

// Returns true if the value was successfully set, meaning |value| was found in
// the list of select options in |field|. Optionally, the caller may pass
// |best_match_index| which will be set to the index of the best match.
bool SetSelectControlValue(const std::u16string& value,
                           FormFieldData* field,
                           size_t* best_match_index,
                           std::string* failure_to_fill) {
  l10n::CaseInsensitiveCompare compare;

  std::u16string best_match;
  for (size_t i = 0; i < field->options.size(); ++i) {
    const SelectOption& option = field->options[i];
    if (value == option.value || value == option.content) {
      // An exact match, use it.
      best_match = option.value;
      if (best_match_index)
        *best_match_index = i;
      break;
    }

    if (compare.StringsEqual(value, option.value) ||
        compare.StringsEqual(value, option.content)) {
      // A match, but not in the same case. Save it in case an exact match is
      // not found.
      best_match = option.value;
      if (best_match_index)
        *best_match_index = i;
    }
  }

  if (best_match.empty()) {
    if (failure_to_fill) {
      *failure_to_fill +=
          "Did not find value to fill in select control element. ";
    }
    return false;
  }

  field->value = best_match;
  return true;
}

// Like SetSelectControlValue, but searches within the field values and options
// for |value|. For example, "NC - North Carolina" would match "north carolina".
bool SetSelectControlValueSubstringMatch(const std::u16string& value,
                                         bool ignore_whitespace,
                                         FormFieldData* field,
                                         std::string* failure_to_fill) {
  int best_match = FieldFiller::FindShortestSubstringMatchInSelect(
      value, ignore_whitespace, field);

  if (best_match >= 0) {
    field->value = field->options[best_match].value;
    return true;
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find substring match for filling select control element. ";
  }

  return false;
}

// Like SetSelectControlValue, but searches within the field values and options
// for |value|. First it tokenizes the options, then tries to match against
// tokens. For example, "NC - North Carolina" would match "nc" but not "ca".
bool SetSelectControlValueTokenMatch(const std::u16string& value,
                                     FormFieldData* field,
                                     std::string* failure_to_fill) {
  std::vector<std::u16string> tokenized;
  l10n::CaseInsensitiveCompare compare;
  for (const SelectOption& option : field->options) {
    tokenized =
        base::SplitString(option.value, base::kWhitespaceASCIIAs16,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::find_if(tokenized.begin(), tokenized.end(),
                     [&compare, value](std::u16string& rhs) {
                       return compare.StringsEqual(value, rhs);
                     }) != tokenized.end()) {
      field->value = option.value;
      return true;
    }

    tokenized =
        base::SplitString(option.content, base::kWhitespaceASCIIAs16,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::find_if(tokenized.begin(), tokenized.end(),
                     [&compare, value](std::u16string& rhs) {
                       return compare.StringsEqual(value, rhs);
                     }) != tokenized.end()) {
      field->value = option.value;
      return true;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find token match for filling select control element. ";
  }

  return false;
}

// Helper method to normalize the |admin_area| for the given |country_code|.
// The value in |admin_area| will be overwritten.
bool NormalizeAdminAreaForCountryCode(std::u16string* admin_area,
                                      const std::string& country_code,
                                      AddressNormalizer* address_normalizer) {
  DCHECK(address_normalizer);
  DCHECK(admin_area);
  if (admin_area->empty() || country_code.empty())
    return false;

  AutofillProfile tmp_profile;
  tmp_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  tmp_profile.SetRawInfo(ADDRESS_HOME_STATE, *admin_area);
  if (!address_normalizer->NormalizeAddressSync(&tmp_profile))
    return false;

  *admin_area = tmp_profile.GetRawInfo(ADDRESS_HOME_STATE);
  return true;
}

// Will normalize |value| and the options in |field| with |address_normalizer|
// (which should not be null), and return whether the fill was successful.
bool SetNormalizedStateSelectControlValue(const std::u16string& value,
                                          FormFieldData* field,
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
    if (failure_to_fill)
      *failure_to_fill += "Could not normalize admin area for country code. ";
    return false;
  }

  // If successful, try filling the normalized value with the existing field
  // |options|.
  if (SetSelectControlValue(field_value, field, /*best_match_index=*/nullptr,
                            failure_to_fill))
    return true;

  // Normalize the |field| options in place, using a copy.
  // TODO(crbug.com/788417): We should probably sanitize the values in
  // |field_copy| before normalizing.
  FormFieldData field_copy(*field);
  bool normalized = false;
  for (SelectOption& option : field_copy.options) {
    normalized |= NormalizeAdminAreaForCountryCode(&option.value, country_code,
                                                   address_normalizer);
    normalized |= NormalizeAdminAreaForCountryCode(
        &option.content, country_code, address_normalizer);
  }

  // If at least some normalization happened on the field options, try filling
  // them with |field_value|.
  size_t best_match_index = 0;
  if (normalized && SetSelectControlValue(field_value, &field_copy,
                                          &best_match_index, failure_to_fill)) {
    // |best_match_index| now points to the option in |field->options|
    // that corresponds to our best match. Update |field| with the answer.
    field->value = field->options[best_match_index].value;
    return true;
  }

  if (failure_to_fill)
    *failure_to_fill += "Could not set normalized state in control element. ";

  return false;
}

// Try to fill a numeric |value| into the given |field|.
bool FillNumericSelectControl(int value,
                              FormFieldData* field,
                              std::string* failure_to_fill) {
  for (const SelectOption& option : field->options) {
    int num;
    if ((StringToInt(option.value, &num) && num == value) ||
        (StringToInt(option.content, &num) && num == value)) {
      field->value = option.value;
      return true;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find numeric value to fill in select control element. ";
  }
  return false;
}

bool FillStateSelectControl(const std::u16string& value,
                            FormFieldData* field,
                            const std::string& country_code,
                            AddressNormalizer* address_normalizer,
                            std::string* failure_to_fill) {
  std::vector<std::u16string> abbreviations;
  std::vector<std::u16string> full_names;

  if (base::FeatureList::IsEnabled(
          features::kAutofillUseAlternativeStateNameMap)) {
    // Fetch the corresponding entry from AlternativeStateNameMap.
    absl::optional<StateEntry> state_entry =
        AlternativeStateNameMap::GetInstance()->GetEntry(
            AlternativeStateNameMap::CountryCode(country_code),
            AlternativeStateNameMap::StateName(value));
    if (state_entry) {
      for (const auto& abbr : state_entry->abbreviations())
        abbreviations.push_back(base::UTF8ToUTF16(abbr));
      if (state_entry->has_canonical_name())
        full_names.push_back(base::UTF8ToUTF16(state_entry->canonical_name()));
      for (const auto& alternative_name : state_entry->alternative_names())
        full_names.push_back(base::UTF8ToUTF16(alternative_name));
    } else {
      full_names.push_back(value);
    }
  }

  std::u16string full;
  std::u16string abbreviation;
  // |abbreviation| will be empty for non-US countries.
  state_names::GetNameAndAbbreviation(value, &full, &abbreviation);

  if (!full.empty())
    full_names.push_back(std::move(full));

  if (!abbreviation.empty())
    abbreviations.push_back(std::move(abbreviation));

  // Try an exact match of the abbreviation first.
  for (const auto& abbreviation : abbreviations) {
    if (!abbreviation.empty() &&
        SetSelectControlValue(abbreviation, field, /*best_match_index=*/nullptr,
                              failure_to_fill)) {
      return true;
    }
  }

  // Try an exact match of the full name.
  for (const auto& full : full_names) {
    if (!full.empty() &&
        SetSelectControlValue(full, field, /*best_match_index=*/nullptr,
                              failure_to_fill)) {
      return true;
    }
  }

  // Try an inexact match of the full name.
  for (const auto& full : full_names) {
    if (!full.empty() && SetSelectControlValueSubstringMatch(full, false, field,
                                                             failure_to_fill)) {
      return true;
    }
  }

  // Try an inexact match of the abbreviation name.
  for (const auto& abbreviation : abbreviations) {
    if (!abbreviation.empty() &&
        SetSelectControlValueTokenMatch(abbreviation, field, failure_to_fill)) {
      return true;
    }
  }

  // Try to match a normalized |value| of the state and the |field| options.
  if (address_normalizer &&
      SetNormalizedStateSelectControlValue(
          value, field, country_code, address_normalizer, failure_to_fill)) {
    return true;
  }

  if (failure_to_fill)
    *failure_to_fill += "Could not fill state in select control element. ";

  return false;
}

bool FillCountrySelectControl(const std::u16string& value,
                              FormFieldData* field_data,
                              std::string* failure_to_fill) {
  std::string country_code = CountryNames::GetInstance()->GetCountryCode(value);
  if (country_code.empty()) {
    if (failure_to_fill)
      *failure_to_fill += "Cannot fill empty country code. ";
    return false;
  }

  for (const SelectOption& option : field_data->options) {
    // Canonicalize each <option> value to a country code, and compare to the
    // target country code.
    if (country_code ==
            CountryNames::GetInstance()->GetCountryCode(option.value) ||
        country_code ==
            CountryNames::GetInstance()->GetCountryCode(option.content)) {
      field_data->value = option.value;
      return true;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find country to fill in select control element. ";
  }
  return false;
}

// Attempt to fill the user's expiration month |value| inside the <select>
// |field|. Since |value| is well defined but the website's |field| option
// values may not be, some heuristics are run to cover all observed cases.
bool FillExpirationMonthSelectControl(const std::u16string& value,
                                      const std::string& app_locale,
                                      FormFieldData* field,
                                      std::string* failure_to_fill) {
  // |value| is defined to be between 1 and 12, inclusively.
  int month = 0;
  if (!StringToInt(value, &month) || month <= 0 || month > 12) {
    if (failure_to_fill)
      *failure_to_fill += "Cannot parse month, or value is <=0 or >12. ";
    return false;
  }

  // Trim the whitespace and specific prefixes used in AngularJS from the
  // select values before attempting to convert them to months.
  std::vector<std::u16string> trimmed_values(field->options.size());
  const std::u16string kNumberPrefix = u"number:";
  const std::u16string kStringPrefix = u"string:";
  for (size_t i = 0; i < field->options.size(); ++i) {
    base::TrimWhitespace(field->options[i].value, base::TRIM_ALL,
                         &trimmed_values[i]);
    base::ReplaceFirstSubstringAfterOffset(&trimmed_values[i], 0, kNumberPrefix,
                                           u"");
    base::ReplaceFirstSubstringAfterOffset(&trimmed_values[i], 0, kStringPrefix,
                                           u"");
  }

  if (trimmed_values.size() == 12) {
    // The select presumable only contains the year's months.
    // If the first value of the select is 0, decrement the value of |month| so
    // January is associated with 0 instead of 1.
    int first_value;
    if (StringToInt(trimmed_values[0], &first_value) && first_value == 0)
      --month;
  } else if (trimmed_values.size() == 13) {
    // The select presumably uses the first value as a placeholder.
    int first_value;
    // If the first value is not a number or is a negative one, check the second
    // value and apply the same logic as if there was no placeholder.
    if (!StringToInt(trimmed_values[0], &first_value) || first_value < 0) {
      int second_value;
      if (StringToInt(trimmed_values[1], &second_value) && second_value == 0)
        --month;
    } else if (first_value == 1) {
      // If the first value of the select is 1, increment the value of |month|
      // to skip the placeholder value (January = 2).
      ++month;
    }
  }

  // Attempt to match the user's |month| with the field's value attributes.
  DCHECK_EQ(trimmed_values.size(), field->options.size());
  for (size_t i = 0; i < trimmed_values.size(); ++i) {
    int converted_value = 0;
    // We use the trimmed value to match with |month|, but the original select
    // value to fill the field (otherwise filling wouldn't work).
    if (data_util::ParseExpirationMonth(trimmed_values[i], app_locale,
                                        &converted_value) &&
        month == converted_value) {
      field->value = field->options[i].value;
      return true;
    }
  }

  // Attempt to match with each of the options' content.
  for (const SelectOption& option : field->options) {
    int converted_contents = 0;
    if (data_util::ParseExpirationMonth(option.content, app_locale,
                                        &converted_contents) &&
        month == converted_contents) {
      field->value = option.value;
      return true;
    }
  }

  return FillNumericSelectControl(month, field, failure_to_fill);
}

// Returns true if the last two digits in |year| match those in |str|.
bool LastTwoDigitsMatch(const std::u16string& year, const std::u16string& str) {
  int year_int;
  int str_int;
  if (!StringToInt(year, &year_int) || !StringToInt(str, &str_int))
    return false;

  return (year_int % 100) == (str_int % 100);
}

// Try to fill a year |value| into the given |field| by comparing the last two
// digits of the year to the field's options.
bool FillYearSelectControl(const std::u16string& value,
                           FormFieldData* field,
                           std::string* failure_to_fill) {
  if (value.size() != 2U && value.size() != 4U) {
    if (failure_to_fill)
      *failure_to_fill += "Year to fill does not have length 2 or 4. ";
    return false;
  }

  for (const SelectOption& option : field->options) {
    if (LastTwoDigitsMatch(value, option.value) ||
        LastTwoDigitsMatch(value, option.content)) {
      field->value = option.value;
      return true;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Year to fill was not found in select control element. ";
  }

  return false;
}

// Try to fill a credit card type |value| (Visa, Mastercard, etc.) into the
// given |field|. We ignore whitespace when filling credit card types to
// allow for cases such as "Master card".

bool FillCreditCardTypeSelectControl(const std::u16string& value,
                                     FormFieldData* field,
                                     std::string* failure_to_fill) {
  if (SetSelectControlValueSubstringMatch(value, /* ignore_whitespace= */ true,
                                          field, failure_to_fill)) {
    return true;
  }
  if (value == l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX)) {
    // For American Express, also try filling as "AmEx".
    return SetSelectControlValueSubstringMatch(u"AmEx",
                                               /* ignore_whitespace= */ true,
                                               field, failure_to_fill);
  }

  if (failure_to_fill)
    *failure_to_fill += "Failed to fill credit card type. ";
  return false;
}

// Set |field_data|'s value to |number|, or |phone_home_city_and_number|, or
// possibly an appropriate substring of |number|.  The |field| specifies the
// type of the phone and whether this is a phone prefix or suffix.
void FillPhoneNumberField(const AutofillField& field,
                          const std::u16string& number,
                          const std::u16string& phone_home_city_and_number,
                          FormFieldData* field_data) {
  field_data->value = FieldFiller::GetPhoneNumberValue(
      field, number, phone_home_city_and_number, *field_data);
}

// Set |field_data|'s value to |number|, or possibly an appropriate substring
// of |number| for cases where credit card number splits across multiple HTML
// form input fields.
// The |field| specifies the |credit_card_number_offset_| to the substring
// within credit card number.
void FillCreditCardNumberField(const AutofillField& field,
                               const std::u16string& number,
                               FormFieldData* field_data) {
  std::u16string value = number;

  // |field|'s max_length truncates credit card number to fit within.
  if (field.credit_card_number_offset() < number.length()) {
    field_data->value = number.substr(
        field.credit_card_number_offset(),
        field.max_length > 0 ? field.max_length : std::u16string::npos);
  } else {
    // If the offset exceeds the length of the number, simply fill the whole
    // number. By this, a wrongly detected second credit card number field
    // before the actual field will not prevent the filling.
    field_data->value = number;
  }
}

// Fills in the select control |field| with |value|. If an exact match is not
// found, falls back to alternate filling strategies based on the |type|.
bool FillSelectControl(const AutofillType& type,
                       const std::u16string& value,
                       absl::variant<const AutofillProfile*, const CreditCard*>
                           profile_or_credit_card,
                       const std::string& app_locale,
                       FormFieldData* field,
                       AddressNormalizer* address_normalizer,
                       std::string* failure_to_fill) {
  DCHECK_EQ("select-one", field->form_control_type);

  ServerFieldType storable_type = type.GetStorableType();

  // Credit card expiration month is checked first since an exact match on value
  // may not be correct.
  if (storable_type == CREDIT_CARD_EXP_MONTH)
    return FillExpirationMonthSelectControl(value, app_locale, field,
                                            failure_to_fill);

  // Search for exact matches.
  if (SetSelectControlValue(value, field, /*best_match_index=*/nullptr,
                            failure_to_fill))
    return true;

  // If that fails, try specific fallbacks based on the field type.
  if (storable_type == ADDRESS_HOME_STATE) {
    DCHECK(absl::holds_alternative<const AutofillProfile*>(
        profile_or_credit_card));
    const std::string country_code = data_util::GetCountryCodeWithFallback(
        *absl::get<const AutofillProfile*>(profile_or_credit_card), app_locale);
    return FillStateSelectControl(value, field, country_code,
                                  address_normalizer, failure_to_fill);
  }
  if (storable_type == ADDRESS_HOME_COUNTRY)
    return FillCountrySelectControl(value, field, failure_to_fill);
  if (storable_type == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
      storable_type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
    return FillYearSelectControl(value, field, failure_to_fill);
  }
  if (storable_type == CREDIT_CARD_TYPE)
    return FillCreditCardTypeSelectControl(value, field, failure_to_fill);

  return false;
}

// Fills in the month control |field| with the expiration date in |card|.
void FillMonthControl(const CreditCard& card, FormFieldData* field) {
  field->value = card.Expiration4DigitYearAsString() + u"-" +
                 card.Expiration2DigitMonthAsString();
}

// Fills |field| with the street address in |value|. Translates newlines into
// equivalent separators when necessary, i.e. when filling a single-line field.
// The separators depend on |address_language_code|.
void FillStreetAddress(const std::u16string& value,
                       const std::string& address_language_code,
                       FormFieldData* field) {
  if (field->form_control_type == "textarea") {
    field->value = value;
    return;
  }

  ::i18n::addressinput::AddressData address_data;
  address_data.language_code = address_language_code;
  address_data.address_line =
      base::SplitString(base::UTF16ToUTF8(value), "\n", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  std::string line;
  ::i18n::addressinput::GetStreetAddressLinesAsSingleLine(address_data, &line);
  field->value = base::UTF8ToUTF16(line);
}

// Returns whether the |field| was filled with the state in |value| or its
// canonical state name or the abbreviation.
// First looks if |value| fits directly in the field, then looks if the
// abbreviation of |value| fits in case the
// |features::kAutofillUseAlternativeStateNameMap| is disabled.
// If the |features::kAutofillUseAlternativeStateNameMap| is enabled, the
// canonical state is checked if it fits in the field and at last the
// abbreviations are tried.
// Does not fill if neither |value| nor the canonical state name nor its
// abbreviation fit into the field.
bool FillStateText(const std::u16string& value,
                   const std::string& country_code,
                   FormFieldData* field,
                   std::string* failure_to_fill) {
  if (field->max_length == 0 || field->max_length >= value.size()) {
    // Fill the state value directly.
    field->value = value;
    return true;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillUseAlternativeStateNameMap)) {
    absl::optional<StateEntry> state =
        AlternativeStateNameMap::GetInstance()->GetEntry(
            AlternativeStateNameMap::CountryCode(country_code),
            AlternativeStateNameMap::StateName(value));
    if (state) {
      // Fill with the canonical state name if possible.
      if (state->has_canonical_name() && !state->canonical_name().empty() &&
          field->max_length >= state->canonical_name().size()) {
        field->value = base::UTF8ToUTF16(state->canonical_name());
        return true;
      }

      // Fill with the abbreviation if possible.
      for (const auto& abbr : state->abbreviations()) {
        if (!abbr.empty() && field->max_length >= abbr.size()) {
          field->value = base::i18n::ToUpper(base::UTF8ToUTF16(abbr));
          return true;
        }
      }
    }
  }

  // Fill with the state abbreviation.
  std::u16string abbreviation;
  state_names::GetNameAndAbbreviation(value, nullptr, &abbreviation);
  if (!abbreviation.empty() && field->max_length >= abbreviation.size()) {
    field->value = base::i18n::ToUpper(abbreviation);
    return true;
  }

  if (failure_to_fill)
    *failure_to_fill += "Could not fit raw state nor abbreviation. ";
  return false;
}

// Fills the expiration year |value| into the |field|. Uses the |field_type|
// and the |field|'s max_length attribute to determine if the |value| needs to
// be truncated.
void FillExpirationYearInput(std::u16string value,
                             ServerFieldType field_type,
                             FormFieldData* field) {
  // If the |field_type| requires only 2 digits, keep only the last 2 digits of
  // |value|.
  if (field_type == CREDIT_CARD_EXP_2_DIGIT_YEAR && value.length() > 2)
    value = value.substr(value.length() - 2, 2);

  if (field->max_length == 0 || field->max_length >= value.size()) {
    // No length restrictions, fill the year value directly.
    field->value = value;
  } else {
    // Truncate the front of |value| to keep only the number of characters equal
    // to the |field|'s max length.
    field->value =
        value.substr(value.length() - field->max_length, field->max_length);
  }
}

// Returns whether the expiration date |value| was filled into the |field|.
// Uses the |field|'s max_length attribute to determine if the |value| needs to
// be truncated. |value| should be a date formatted as either MM/YY or MM/YYYY.
// If it isn't, the field doesn't get filled.
bool FillExpirationDateInput(const std::u16string& value,
                             FormFieldData* field,
                             std::string* failure_to_fill) {
  const std::u16string kSeparator = u"/";
  // Autofill formats a combined date as month/year.
  std::vector<std::u16string> pieces = base::SplitString(
      value, kSeparator, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (pieces.size() != 2) {
    if (failure_to_fill)
      *failure_to_fill += "Could not split expiration into two pieces. ";
    return false;
  }

  std::u16string month = pieces[0];
  std::u16string year = pieces[1];
  if (month.length() != 2 || (year.length() != 2 && year.length() != 4)) {
    if (failure_to_fill)
      *failure_to_fill += "Unexpected length of month or year to fill. ";
    return false;
  }

  switch (field->max_length) {
    case 1:
    case 2:
    case 3:
      if (failure_to_fill)
        *failure_to_fill += "Field to fill had a lenght of 1-3. ";
      return false;
    case 4:
      // Field likely expects MMYY
      if (year.length() != 2) {
        // Shorten year to 2 characters from 4
        year = year.substr(2);
      }

      field->value = month + year;
      break;
    case 5:
      // Field likely expects MM/YY
      if (year.length() != 2) {
        // Shorten year to 2 characters
        year = year.substr(2);
        field->value = month + kSeparator + year;
      } else {
        field->value = value;
      }
      break;
    case 6:
    case 7:
      if (year.length() != 4) {
        // Will normalize 2-digit years to the 4-digit version.
        year = u"20" + year;
      }

      if (field->max_length == 6) {
        // Field likely expects MMYYYY
        field->value = month + year;
      } else {
        // Field likely expects MM/YYYY
        field->value = month + kSeparator + year;
      }
      break;
    default:
      // Includes the case where max_length is not specified (0).
      field->value = month + kSeparator + year;
  }

  return true;
}

std::u16string RemoveWhitespace(const std::u16string& value) {
  std::u16string stripped_value;
  base::RemoveChars(value, base::kWhitespaceUTF16, &stripped_value);
  return stripped_value;
}

// Finds the best suitable option in the |field| that corresponds to the
// |country_code|.
// If the exact match is not found, extracts the digits (ignoring leading '00'
// or '+') from each option and compares them with the |country_code|.
void FillPhoneCountryCodeSelectControl(const std::u16string& country_code,
                                       FormFieldData* field,
                                       std::string* failure_to_fill) {
  if (country_code.empty())
    return;

  // Find the option that exactly matches the |country_code|.
  if (SetSelectControlValue(country_code, field, /*best_match_index=*/nullptr,
                            failure_to_fill))
    return;

  for (const SelectOption& option : field->options) {
    std::u16string cc_candidate_in_value =
        data_util::FindPossiblePhoneCountryCode(RemoveWhitespace(option.value));
    std::u16string cc_candidate_in_content =
        data_util::FindPossiblePhoneCountryCode(
            RemoveWhitespace(option.content));
    if (cc_candidate_in_value == country_code ||
        cc_candidate_in_content == country_code) {
      field->value = option.value;
      return;
    }
  }
}

}  // namespace

FieldFiller::FieldFiller(const std::string& app_locale,
                         AddressNormalizer* address_normalizer)
    : app_locale_(app_locale), address_normalizer_(address_normalizer) {}

FieldFiller::~FieldFiller() {}

bool FieldFiller::FillFormField(
    const AutofillField& field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    FormFieldData* field_data,
    const std::u16string& cvc,
    mojom::RendererFormDataAction action,
    std::string* failure_to_fill) {
  const AutofillType type = field.Type();

  std::u16string value;
  if (absl::holds_alternative<const AutofillProfile*>(profile_or_credit_card)) {
    const AutofillProfile* profile =
        absl::get<const AutofillProfile*>(profile_or_credit_card);
    if (profile->ShouldSkipFillingOrSuggesting(type.GetStorableType())) {
      if (failure_to_fill)
        *failure_to_fill += "ShouldSkipFillingOrSuggesting() returned true. ";
      return false;
    }

    value = profile->GetInfo(type, app_locale_);
  } else {
    DCHECK(absl::holds_alternative<const CreditCard*>(profile_or_credit_card));

    if (type.GetStorableType() == CREDIT_CARD_VERIFICATION_CODE) {
      value = cvc;
    } else if (type.GetStorableType() == CREDIT_CARD_NUMBER &&
               action == mojom::RendererFormDataAction::kPreview) {
      const CreditCard* credit_card =
          absl::get<const CreditCard*>(profile_or_credit_card);
      value = credit_card->ObfuscatedLastFourDigits();
    } else {
      value = absl::get<const CreditCard*>(profile_or_credit_card)
                  ->GetInfo(type, app_locale_);
    }
  }

  // Do not attempt to fill empty values as it would skew the metrics.
  if (value.empty()) {
    if (failure_to_fill)
      *failure_to_fill += "No value to fill available. ";
    return false;
  }

  if (type.group() == FieldTypeGroup::kPhoneHome) {
    // If the |field_data| is a selection box and having the type
    // |PHONE_HOME_COUNTRY_CODE|, call |FillPhoneCountryCodeSelectControl|.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableAugmentedPhoneCountryCode) &&
        field_data->form_control_type == "select-one" &&
        type.GetStorableType() == PHONE_HOME_COUNTRY_CODE) {
      FillPhoneCountryCodeSelectControl(value, field_data, failure_to_fill);
    } else {
      DCHECK(absl::holds_alternative<const AutofillProfile*>(
          profile_or_credit_card));
      const std::u16string phone_home_city_and_number =
          absl::get<const AutofillProfile*>(profile_or_credit_card)
              ->GetInfo(PHONE_HOME_CITY_AND_NUMBER, app_locale_);
      FillPhoneNumberField(field, value, phone_home_city_and_number,
                           field_data);
    }
    return true;
  }
  if (field_data->form_control_type == "select-one") {
    return FillSelectControl(type, value, profile_or_credit_card, app_locale_,
                             field_data, address_normalizer_, failure_to_fill);
  }
  if (field_data->form_control_type == "month") {
    DCHECK(absl::holds_alternative<const CreditCard*>(profile_or_credit_card));
    FillMonthControl(*absl::get<const CreditCard*>(profile_or_credit_card),
                     field_data);
    return true;
  }
  if (type.GetStorableType() == ADDRESS_HOME_STREET_ADDRESS) {
    DCHECK(absl::holds_alternative<const AutofillProfile*>(
        profile_or_credit_card));
    const std::string& profile_language_code =
        absl::get<const AutofillProfile*>(profile_or_credit_card)
            ->language_code();
    FillStreetAddress(value, profile_language_code, field_data);
    return true;
  }
  if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
    FillCreditCardNumberField(field, value, field_data);
    return true;
  }
  if (type.GetStorableType() == ADDRESS_HOME_STATE) {
    DCHECK(absl::holds_alternative<const AutofillProfile*>(
        profile_or_credit_card));
    const std::string& country_code = data_util::GetCountryCodeWithFallback(
        *absl::get<const AutofillProfile*>(profile_or_credit_card),
        app_locale_);
    return FillStateText(value, country_code, field_data, failure_to_fill);
  }
  if (field_data->form_control_type == "text" &&
      (type.GetStorableType() == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
       type.GetStorableType() == CREDIT_CARD_EXP_4_DIGIT_YEAR)) {
    FillExpirationYearInput(value, type.GetStorableType(), field_data);
    return true;
  }
  if (field_data->form_control_type == "text" &&
      (type.GetStorableType() == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
       type.GetStorableType() == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)) {
    return FillExpirationDateInput(value, field_data, failure_to_fill);
  }

  field_data->value = value;
  return true;
}

// TODO(crbug.com/581514): Add support for filling only the prefix/suffix for
// phone numbers with 10 or 11 digits.
// static
std::u16string FieldFiller::GetPhoneNumberValue(
    const AutofillField& field,
    const std::u16string& number,
    const std::u16string& phone_home_city_and_number,
    const FormFieldData& field_data) {
  // TODO(crbug.com/581485): Investigate the use of libphonenumber here.
  // Check to see if the |field| size matches the "prefix" or "suffix" size or
  // if the field was labeled as such. If so, return the appropriate substring.
  if (number.length() ==
      PhoneNumber::kPrefixLength + PhoneNumber::kSuffixLength) {
    if (field.phone_part() == AutofillField::PHONE_PREFIX ||
        field_data.max_length == PhoneNumber::kPrefixLength) {
      return number.substr(PhoneNumber::kPrefixOffset,
                           PhoneNumber::kPrefixLength);
    }

    if (field.phone_part() == AutofillField::PHONE_SUFFIX ||
        field_data.max_length == PhoneNumber::kSuffixLength) {
      return number.substr(PhoneNumber::kSuffixOffset,
                           PhoneNumber::kSuffixLength);
    }
  }

  // If no max length was specified, return the complete number.
  if (field_data.max_length == 0)
    return number;

  if (number.length() > field_data.max_length) {
    // Try after removing the country code, if |number| exceeds the maximum size
    // of the field.
    if (phone_home_city_and_number.length() <= field_data.max_length)
      return phone_home_city_and_number;

    // If |number| exceeds the maximum size of the field, cut the first part to
    // provide a valid number for the field. For example, the number 15142365264
    // with a field with a max length of 10 would return 5142365264, thus
    // filling in the last |field_data.max_length| characters from the |number|.
    return number.substr(number.length() - field_data.max_length,
                         field_data.max_length);
  }

  return number;
}

// static
int FieldFiller::FindShortestSubstringMatchInSelect(
    const std::u16string& value,
    bool ignore_whitespace,
    const FormFieldData* field) {
  int best_match = -1;

  std::u16string value_stripped =
      ignore_whitespace ? RemoveWhitespace(value) : value;
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents searcher(
      value_stripped);
  for (size_t i = 0; i < field->options.size(); ++i) {
    const SelectOption& option = field->options[i];
    std::u16string option_value =
        ignore_whitespace ? RemoveWhitespace(option.value) : option.value;
    std::u16string option_content =
        ignore_whitespace ? RemoveWhitespace(option.content) : option.content;
    if (searcher.Search(option_value, nullptr, nullptr) ||
        searcher.Search(option_content, nullptr, nullptr)) {
      if (best_match == -1 ||
          field->options[best_match].value.size() > option.value.size()) {
        best_match = i;
      }
    }
  }
  return best_match;
}

}  // namespace autofill
