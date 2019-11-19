// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filler.h"

#include <stdint.h>

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
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
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
bool SetSelectControlValue(const base::string16& value,
                           FormFieldData* field,
                           size_t* best_match_index = nullptr) {
  l10n::CaseInsensitiveCompare compare;

  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  base::string16 best_match;
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    if (value == field->option_values[i] ||
        value == field->option_contents[i]) {
      // An exact match, use it.
      best_match = field->option_values[i];
      if (best_match_index)
        *best_match_index = i;
      break;
    }

    if (compare.StringsEqual(value, field->option_values[i]) ||
        compare.StringsEqual(value, field->option_contents[i])) {
      // A match, but not in the same case. Save it in case an exact match is
      // not found.
      best_match = field->option_values[i];
      if (best_match_index)
        *best_match_index = i;
    }
  }

  if (best_match.empty())
    return false;

  field->value = best_match;
  return true;
}

// Like SetSelectControlValue, but searches within the field values and options
// for |value|. For example, "NC - North Carolina" would match "north carolina".
bool SetSelectControlValueSubstringMatch(const base::string16& value,
                                         bool ignore_whitespace,
                                         FormFieldData* field) {
  int best_match = FieldFiller::FindShortestSubstringMatchInSelect(
      value, ignore_whitespace, field);

  if (best_match >= 0) {
    field->value = field->option_values[best_match];
    return true;
  }

  return false;
}

// Like SetSelectControlValue, but searches within the field values and options
// for |value|. First it tokenizes the options, then tries to match against
// tokens. For example, "NC - North Carolina" would match "nc" but not "ca".
bool SetSelectControlValueTokenMatch(const base::string16& value,
                                     FormFieldData* field) {
  std::vector<base::string16> tokenized;
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  l10n::CaseInsensitiveCompare compare;

  for (size_t i = 0; i < field->option_values.size(); ++i) {
    tokenized =
        base::SplitString(field->option_values[i], base::kWhitespaceASCIIAs16,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::find_if(tokenized.begin(), tokenized.end(),
                     [&compare, value](base::string16& rhs) {
                       return compare.StringsEqual(value, rhs);
                     }) != tokenized.end()) {
      field->value = field->option_values[i];
      return true;
    }

    tokenized =
        base::SplitString(field->option_contents[i], base::kWhitespaceASCIIAs16,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::find_if(tokenized.begin(), tokenized.end(),
                     [&compare, value](base::string16& rhs) {
                       return compare.StringsEqual(value, rhs);
                     }) != tokenized.end()) {
      field->value = field->option_values[i];
      return true;
    }
  }

  return false;
}

// Helper method to normalize the |admin_area| for the given |country_code|.
// The value in |admin_area| will be overwritten.
bool NormalizeAdminAreaForCountryCode(base::string16* admin_area,
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
bool SetNormalizedStateSelectControlValue(
    const base::string16& value,
    FormFieldData* field,
    const std::string& country_code,
    AddressNormalizer* address_normalizer) {
  DCHECK(address_normalizer);
  // We attempt to normalize a copy of the field value. If normalization was not
  // successful, it means the rules were probably not loaded. Give up. Note that
  // the normalizer will fetch the rule for next time it's called.
  // TODO(crbug.com/788417): We should probably sanitize |value| before
  // normalizing.
  base::string16 field_value = value;
  if (!NormalizeAdminAreaForCountryCode(&field_value, country_code,
                                        address_normalizer)) {
    return false;
  }

  // If successful, try filling the normalized value with the existing field
  // |options|.
  if (SetSelectControlValue(field_value, field))
    return true;

  // Normalize the |field| options in place, using a copy.
  // TODO(crbug.com/788417): We should probably sanitize the values in
  // |field_copy| before normalizing.
  FormFieldData field_copy(*field);
  bool normalized = false;
  for (size_t i = 0; i < field_copy.option_values.size(); ++i) {
    normalized |= NormalizeAdminAreaForCountryCode(
        &field_copy.option_values[i], country_code, address_normalizer);
    normalized |= NormalizeAdminAreaForCountryCode(
        &field_copy.option_contents[i], country_code, address_normalizer);
  }

  // If at least some normalization happened on the field options, try filling
  // them with |field_value|.
  size_t best_match_index = 0;
  if (normalized &&
      SetSelectControlValue(field_value, &field_copy, &best_match_index)) {
    // |best_match_index| now points to the option in |field->option_values|
    // that corresponds to our best match. Update |field| with the answer.
    field->value = field->option_values[best_match_index];
    return true;
  }

  return false;
}

// Try to fill a numeric |value| into the given |field|.
bool FillNumericSelectControl(int value, FormFieldData* field) {
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    int option;
    if ((StringToInt(field->option_values[i], &option) && option == value) ||
        (StringToInt(field->option_contents[i], &option) && option == value)) {
      field->value = field->option_values[i];
      return true;
    }
  }

  return false;
}

bool FillStateSelectControl(const base::string16& value,
                            FormFieldData* field,
                            const std::string& country_code,
                            AddressNormalizer* address_normalizer) {
  base::string16 full;
  base::string16 abbreviation;
  // |abbreviation| will be empty for non-US countries.
  state_names::GetNameAndAbbreviation(value, &full, &abbreviation);

  // Try an exact match of the abbreviation first.
  if (!abbreviation.empty() && SetSelectControlValue(abbreviation, field)) {
    return true;
  }

  // Try an exact match of the full name.
  if (!full.empty() && SetSelectControlValue(full, field)) {
    return true;
  }

  // Try an inexact match of the full name.
  if (!full.empty() &&
      SetSelectControlValueSubstringMatch(full, false, field)) {
    return true;
  }

  // Try an inexact match of the abbreviation name.
  if (!abbreviation.empty() &&
      SetSelectControlValueTokenMatch(abbreviation, field)) {
    return true;
  }

  // Try to match a normalized |value| of the state and the |field| options.
  if (address_normalizer &&
      SetNormalizedStateSelectControlValue(value, field, country_code,
                                           address_normalizer)) {
    return true;
  }

  return false;
}

bool FillCountrySelectControl(const base::string16& value,
                              FormFieldData* field_data) {
  std::string country_code = CountryNames::GetInstance()->GetCountryCode(value);
  if (country_code.empty())
    return false;

  DCHECK_EQ(field_data->option_values.size(),
            field_data->option_contents.size());
  for (size_t i = 0; i < field_data->option_values.size(); ++i) {
    // Canonicalize each <option> value to a country code, and compare to the
    // target country code.
    base::string16 value = field_data->option_values[i];
    base::string16 contents = field_data->option_contents[i];
    if (country_code == CountryNames::GetInstance()->GetCountryCode(value) ||
        country_code == CountryNames::GetInstance()->GetCountryCode(contents)) {
      field_data->value = value;
      return true;
    }
  }

  return false;
}

// Attempt to fill the user's expiration month |value| inside the <select>
// |field|. Since |value| is well defined but the website's |field| option
// values may not be, some heuristics are run to cover all observed cases.
bool FillExpirationMonthSelectControl(const base::string16& value,
                                      const std::string& app_locale,
                                      FormFieldData* field) {
  // |value| is defined to be between 1 and 12, inclusively.
  int month = 0;
  if (!StringToInt(value, &month) || month <= 0 || month > 12)
    return false;

  // Trim the whitespace and specific prefixes used in AngularJS from the
  // select values before attempting to convert them to months.
  std::vector<base::string16> trimmed_values(field->option_values.size());
  const base::string16 kNumberPrefix = ASCIIToUTF16("number:");
  const base::string16 kStringPrefix = ASCIIToUTF16("string:");
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    base::TrimWhitespace(field->option_values[i], base::TRIM_ALL,
                         &trimmed_values[i]);
    base::ReplaceFirstSubstringAfterOffset(&trimmed_values[i], 0, kNumberPrefix,
                                           ASCIIToUTF16(""));
    base::ReplaceFirstSubstringAfterOffset(&trimmed_values[i], 0, kStringPrefix,
                                           ASCIIToUTF16(""));
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
  for (size_t i = 0; i < trimmed_values.size(); ++i) {
    int converted_value = 0;
    // We use the trimmed value to match with |month|, but the original select
    // value to fill the field (otherwise filling wouldn't work).
    if (CreditCard::ConvertMonth(trimmed_values[i], app_locale,
                                 &converted_value) &&
        month == converted_value) {
      field->value = field->option_values[i];
      return true;
    }
  }

  // Attempt to match with each of the options' content.
  for (size_t i = 0; i < field->option_contents.size(); ++i) {
    int converted_contents = 0;
    if (CreditCard::ConvertMonth(field->option_contents[i], app_locale,
                                 &converted_contents) &&
        month == converted_contents) {
      field->value = field->option_values[i];
      return true;
    }
  }

  return FillNumericSelectControl(month, field);
}

// Returns true if the last two digits in |year| match those in |str|.
bool LastTwoDigitsMatch(const base::string16& year, const base::string16& str) {
  int year_int;
  int str_int;
  if (!StringToInt(year, &year_int) || !StringToInt(str, &str_int))
    return false;

  return (year_int % 100) == (str_int % 100);
}

// Try to fill a year |value| into the given |field| by comparing the last two
// digits of the year to the field's options.
bool FillYearSelectControl(const base::string16& value, FormFieldData* field) {
  if (value.size() != 2U && value.size() != 4U)
    return false;

  DCHECK_EQ(field->option_values.size(), field->option_contents.size());
  for (size_t i = 0; i < field->option_values.size(); ++i) {
    if (LastTwoDigitsMatch(value, field->option_values[i]) ||
        LastTwoDigitsMatch(value, field->option_contents[i])) {
      field->value = field->option_values[i];
      return true;
    }
  }

  return false;
}

// Try to fill a credit card type |value| (Visa, Mastercard, etc.) into the
// given |field|. We ignore whitespace when filling credit card types to
// allow for cases such as "Master card".

bool FillCreditCardTypeSelectControl(const base::string16& value,
                                     FormFieldData* field) {
  if (SetSelectControlValueSubstringMatch(value, /* ignore_whitespace= */ true,
                                          field)) {
    return true;
  }
  if (value == l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX)) {
    // For American Express, also try filling as "AmEx".
    return SetSelectControlValueSubstringMatch(
        ASCIIToUTF16("AmEx"), /* ignore_whitespace= */ true, field);
  }
  return false;
}

// Set |field_data|'s value to |number|, or possibly an appropriate substring of
// |number|.  The |field| specifies the type of the phone and whether this is a
// phone prefix or suffix.
void FillPhoneNumberField(const AutofillField& field,
                          const base::string16& number,
                          FormFieldData* field_data) {
  field_data->value =
      FieldFiller::GetPhoneNumberValue(field, number, *field_data);
}

// Set |field_data|'s value to |number|, or possibly an appropriate substring
// of |number| for cases where credit card number splits across multiple HTML
// form input fields.
// The |field| specifies the |credit_card_number_offset_| to the substring
// within credit card number.
void FillCreditCardNumberField(const AutofillField& field,
                               const base::string16& number,
                               FormFieldData* field_data) {
  base::string16 value = number;

  // |field|'s max_length truncates credit card number to fit within.
  if (field.credit_card_number_offset() < value.length())
    value = value.substr(field.credit_card_number_offset());

  field_data->value = value;
}

// Fills in the select control |field| with |value|. If an exact match is not
// found, falls back to alternate filling strategies based on the |type|.
bool FillSelectControl(const AutofillType& type,
                       const base::string16& value,
                       FormFieldData* field,
                       const AutofillDataModel& data_model,
                       const std::string& app_locale,
                       AddressNormalizer* address_normalizer) {
  DCHECK_EQ("select-one", field->form_control_type);

  // Guard against corrupted values passed over IPC.
  if (field->option_values.size() != field->option_contents.size())
    return false;

  ServerFieldType storable_type = type.GetStorableType();

  // Credit card expiration month is checked first since an exact match on value
  // may not be correct.
  if (storable_type == CREDIT_CARD_EXP_MONTH)
    return FillExpirationMonthSelectControl(value, app_locale, field);

  // Search for exact matches.
  if (SetSelectControlValue(value, field))
    return true;

  // If that fails, try specific fallbacks based on the field type.
  if (storable_type == ADDRESS_HOME_STATE) {
    // Safe to cast the data model to AutofillProfile here.
    const std::string country_code = data_util::GetCountryCodeWithFallback(
        static_cast<const AutofillProfile&>(data_model), app_locale);
    return FillStateSelectControl(value, field, country_code,
                                  address_normalizer);
  }
  if (storable_type == ADDRESS_HOME_COUNTRY)
    return FillCountrySelectControl(value, field);
  if (storable_type == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
      storable_type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
    return FillYearSelectControl(value, field);
  }
  if (storable_type == CREDIT_CARD_TYPE)
    return FillCreditCardTypeSelectControl(value, field);

  return false;
}

// Fills in the month control |field| with the expiration date in |card|.
void FillMonthControl(const CreditCard& card, FormFieldData* field) {
  field->value = card.Expiration4DigitYearAsString() + ASCIIToUTF16("-") +
                 card.ExpirationMonthAsString();
}

// Fills |field| with the street address in |value|. Translates newlines into
// equivalent separators when necessary, i.e. when filling a single-line field.
// The separators depend on |address_language_code|.
void FillStreetAddress(const base::string16& value,
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
// abbreviation. First looks if |value| fits directly in the field, then looks
// if the abbreviation of |value| fits. Does not fill if neither |value| or its
// abbreviation are too long for the field.
bool FillStateText(const base::string16& value, FormFieldData* field) {
  if (field->max_length == 0 || field->max_length >= value.size()) {
    // Fill the state value directly.
    field->value = value;
    return true;
  }
  // Fill with the state abbreviation.
  base::string16 abbreviation;
  state_names::GetNameAndAbbreviation(value, nullptr, &abbreviation);
  if (!abbreviation.empty() && field->max_length >= abbreviation.size()) {
    field->value = base::i18n::ToUpper(abbreviation);
    return true;
  }
  return false;
}

// Fills the expiration year |value| into the |field|. Uses the |field_type|
// and the |field|'s max_length attribute to determine if the |value| needs to
// be truncated.
void FillExpirationYearInput(base::string16 value,
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
bool FillExpirationDateInput(const base::string16& value,
                             FormFieldData* field) {
  const base::string16 kSeparator = ASCIIToUTF16("/");
  // Autofill formats a combined date as month/year.
  std::vector<base::string16> pieces = base::SplitString(
      value, kSeparator, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (pieces.size() != 2)
    return false;

  base::string16 month = pieces[0];
  base::string16 year = pieces[1];
  if (month.length() != 2 || (year.length() != 2 && year.length() != 4))
    return false;

  switch (field->max_length) {
    case 1:
    case 2:
    case 3:
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
        year = ASCIIToUTF16("20") + year;
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

base::string16 RemoveWhitespace(const base::string16& value) {
  base::string16 stripped_value;
  base::RemoveChars(value, base::kWhitespaceUTF16, &stripped_value);
  return stripped_value;
}

}  // namespace

FieldFiller::FieldFiller(const std::string& app_locale,
                         AddressNormalizer* address_normalizer)
    : app_locale_(app_locale), address_normalizer_(address_normalizer) {}

FieldFiller::~FieldFiller() {}

bool FieldFiller::FillFormField(const AutofillField& field,
                                const AutofillDataModel& data_model,
                                FormFieldData* field_data,
                                const base::string16& cvc) {
  const AutofillType type = field.Type();

  // Don't fill if autocomplete=off is set on |field| on desktop for non credit
  // card related fields.
  if (!base::FeatureList::IsEnabled(features::kAutofillAlwaysFillAddresses) &&
      !field.should_autocomplete && IsDesktopPlatform() &&
      (type.group() != CREDIT_CARD)) {
    return false;
  }

  if (data_model.ShouldSkipFillingOrSuggesting(type.GetStorableType()))
    return false;

  base::string16 value = data_model.GetInfo(type, app_locale_);
  if (type.GetStorableType() == CREDIT_CARD_VERIFICATION_CODE)
    value = cvc;

  // Do not attempt to fill empty values as it would skew the metrics.
  if (value.empty())
    return false;

  if (type.group() == PHONE_HOME) {
    FillPhoneNumberField(field, value, field_data);
    return true;
  }
  if (field_data->form_control_type == "select-one") {
    return FillSelectControl(type, value, field_data, data_model, app_locale_,
                             address_normalizer_);
  }
  if (field_data->form_control_type == "month") {
    // Safe to cast to CreditCard here because month control type only applying
    // to credit card expirations.
    FillMonthControl(static_cast<const CreditCard&>(data_model), field_data);
    return true;
  }
  if (type.GetStorableType() == ADDRESS_HOME_STREET_ADDRESS) {
    // Safe to cast to AutofillProfile here because of the address |type|.
    const std::string profile_language_code =
        static_cast<const AutofillProfile&>(data_model).language_code();
    FillStreetAddress(value, profile_language_code, field_data);
    return true;
  }
  if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
    FillCreditCardNumberField(field, value, field_data);
    return true;
  }
  if (type.GetStorableType() == ADDRESS_HOME_STATE)
    return FillStateText(value, field_data);
  if (field_data->form_control_type == "text" &&
      (type.GetStorableType() == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
       type.GetStorableType() == CREDIT_CARD_EXP_4_DIGIT_YEAR)) {
    FillExpirationYearInput(value, type.GetStorableType(), field_data);
    return true;
  }
  if (field_data->form_control_type == "text" &&
      (type.GetStorableType() == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
       type.GetStorableType() == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)) {
    return FillExpirationDateInput(value, field_data);
  }

  field_data->value = value;
  return true;
}

// TODO(crbug.com/581514): Add support for filling only the prefix/suffix for
// phone numbers with 10 or 11 digits.
// static
base::string16 FieldFiller::GetPhoneNumberValue(
    const AutofillField& field,
    const base::string16& number,
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

  // If |number| exceeds the maximum size of the field, cut the first part to
  // provide a valid number for the field. For example, the number 15142365264
  // with a field with a max length of 10 would return 5142365264, thus removing
  // the country code and remaining valid.
  if (number.length() > field_data.max_length) {
    return number.substr(number.length() - field_data.max_length,
                         field_data.max_length);
  }

  return number;
}

// static
int FieldFiller::FindShortestSubstringMatchInSelect(
    const base::string16& value,
    bool ignore_whitespace,
    const FormFieldData* field) {
  DCHECK_EQ(field->option_values.size(), field->option_contents.size());

  int best_match = -1;

  base::string16 value_stripped =
      ignore_whitespace ? RemoveWhitespace(value) : value;
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents searcher(
      value_stripped);

  for (size_t i = 0; i < field->option_values.size(); ++i) {
    base::string16 option_value =
        ignore_whitespace ? RemoveWhitespace(field->option_values[i])
                          : field->option_values[i];
    base::string16 option_content =
        ignore_whitespace ? RemoveWhitespace(field->option_contents[i])
                          : field->option_contents[i];
    if (searcher.Search(option_value, nullptr, nullptr) ||
        searcher.Search(option_content, nullptr, nullptr)) {
      if (best_match == -1 || field->option_values[best_match].size() >
                                  field->option_values[i].size()) {
        best_match = i;
      }
    }
  }
  return best_match;
}

}  // namespace autofill
