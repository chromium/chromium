// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/label_formatter_utils.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/validation.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using data_util::ContainsAddress;
using data_util::ContainsEmail;
using data_util::ContainsName;
using data_util::ContainsPhone;

namespace {

// Returns true if all |profiles| have the same value for the data retrieved by
// |get_data|.
bool HaveSameData(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale,
    base::RepeatingCallback<base::string16(const AutofillProfile&,
                                           const std::string&)> get_data,
    base::RepeatingCallback<bool(const base::string16& str1,
                                 const base::string16& str2)> matches) {
  if (profiles.size() <= 1) {
    return true;
  }

  const base::string16 first_profile_data =
      get_data.Run(*profiles[0], app_locale);
  for (size_t i = 1; i < profiles.size(); ++i) {
    const base::string16 current_profile_data =
        get_data.Run(*profiles[i], app_locale);
    if (!matches.Run(first_profile_data, current_profile_data)) {
      return false;
    }
  }
  return true;
}

// Used to avoid having the same lambda in HaveSameEmailAddresses,
// HaveSameFirstNames, HaveSameStreetAddresses.
bool Equals(const base::string16& str1, const base::string16& str2) {
  return str1 == str2;
}

}  // namespace

void AddLabelPartIfNotEmpty(const base::string16& part,
                            std::vector<base::string16>* parts) {
  if (!part.empty()) {
    parts->push_back(part);
  }
}

base::string16 ConstructLabelLine(const std::vector<base::string16>& parts) {
  return base::JoinString(parts, l10n_util::GetStringUTF16(
                                     IDS_AUTOFILL_SUGGESTION_LABEL_SEPARATOR));
}

base::string16 ConstructMobileLabelLine(
    const std::vector<base::string16>& parts) {
  return base::JoinString(
      parts, l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR));
}

bool IsNonStreetAddressPart(ServerFieldType type) {
  switch (type) {
    case ADDRESS_HOME_CITY:
    case ADDRESS_BILLING_CITY:
    case ADDRESS_HOME_ZIP:
    case ADDRESS_BILLING_ZIP:
    case ADDRESS_HOME_STATE:
    case ADDRESS_BILLING_STATE:
    case ADDRESS_HOME_COUNTRY:
    case ADDRESS_BILLING_COUNTRY:
    case ADDRESS_HOME_SORTING_CODE:
    case ADDRESS_BILLING_SORTING_CODE:
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
    case ADDRESS_BILLING_DEPENDENT_LOCALITY:
      return true;
    default:
      return false;
  }
}

bool IsStreetAddressPart(ServerFieldType type) {
  switch (type) {
    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
    case ADDRESS_HOME_APT_NUM:
    case ADDRESS_BILLING_LINE1:
    case ADDRESS_BILLING_LINE2:
    case ADDRESS_BILLING_APT_NUM:
    case ADDRESS_HOME_STREET_ADDRESS:
    case ADDRESS_BILLING_STREET_ADDRESS:
    case ADDRESS_HOME_LINE3:
    case ADDRESS_BILLING_LINE3:
      return true;
    default:
      return false;
  }
}

bool HasNonStreetAddress(const std::vector<ServerFieldType>& types) {
  return std::any_of(types.begin(), types.end(), IsNonStreetAddressPart);
}

bool HasStreetAddress(const std::vector<ServerFieldType>& types) {
  return std::any_of(types.begin(), types.end(), IsStreetAddressPart);
}

std::vector<ServerFieldType> ExtractSpecifiedAddressFieldTypes(
    bool extract_street_address_types,
    const std::vector<ServerFieldType>& types) {
  auto should_be_extracted =
      [&extract_street_address_types](ServerFieldType type) -> bool {
    return AutofillType(AutofillType(type).GetStorableType()).group() ==
               ADDRESS_HOME &&
           (extract_street_address_types ? IsStreetAddressPart(type)
                                         : !IsStreetAddressPart(type));
  };

  std::vector<ServerFieldType> extracted_address_types;
  std::copy_if(types.begin(), types.end(),
               std::back_inserter(extracted_address_types),
               should_be_extracted);

  return extracted_address_types;
}

std::vector<ServerFieldType> ExtractAddressFieldTypes(
    const std::vector<ServerFieldType>& types) {
  std::vector<ServerFieldType> only_address_types;

  // Note that GetStorableType maps billing fields to their corresponding non-
  // billing fields, e.g. ADDRESS_HOME_ZIP is mapped to ADDRESS_BILLING_ZIP.
  std::copy_if(
      types.begin(), types.end(), std::back_inserter(only_address_types),
      [](ServerFieldType type) {
        return AutofillType(AutofillType(type).GetStorableType()).group() ==
               ADDRESS_HOME;
      });
  return only_address_types;
}

std::vector<ServerFieldType> TypesWithoutFocusedField(
    const std::vector<ServerFieldType>& types,
    ServerFieldType field_type_to_remove) {
  std::vector<ServerFieldType> types_without_field;
  std::copy_if(types.begin(), types.end(),
               std::back_inserter(types_without_field),
               [&field_type_to_remove](ServerFieldType type) -> bool {
                 return type != field_type_to_remove;
               });
  return types_without_field;
}

AutofillProfile MakeTrimmedProfile(const AutofillProfile& profile,
                                   const std::string& app_locale,
                                   const std::vector<ServerFieldType>& types) {
  AutofillProfile trimmed_profile(profile.guid(), profile.origin());
  trimmed_profile.set_language_code(profile.language_code());

  const AutofillType country_code_type(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  const base::string16 country_code =
      profile.GetInfo(country_code_type, app_locale);
  trimmed_profile.SetInfo(country_code_type, country_code, app_locale);

  for (const ServerFieldType& type : types) {
    trimmed_profile.SetInfo(type, profile.GetInfo(type, app_locale),
                            app_locale);
  }
  return trimmed_profile;
}

base::string16 GetLabelForFocusedAddress(
    ServerFieldType focused_field_type,
    bool form_has_street_address,
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& types) {
  return GetLabelAddress(
      form_has_street_address && !IsStreetAddressPart(focused_field_type),
      profile, app_locale, types);
}

base::string16 GetLabelAddress(bool use_street_address,
                               const AutofillProfile& profile,
                               const std::string& app_locale,
                               const std::vector<ServerFieldType>& types) {
  return use_street_address
             ? GetLabelStreetAddress(
                   ExtractSpecifiedAddressFieldTypes(use_street_address, types),
                   profile, app_locale)
             : GetLabelNationalAddress(
                   ExtractSpecifiedAddressFieldTypes(use_street_address, types),
                   profile, app_locale);
}

base::string16 GetLabelNationalAddress(
    const std::vector<ServerFieldType>& types,
    const AutofillProfile& profile,
    const std::string& app_locale) {
  std::unique_ptr<::i18n::addressinput::AddressData> address_data =
      i18n::CreateAddressDataFromAutofillProfile(
          MakeTrimmedProfile(profile, app_locale, types), app_locale);

  std::string address_line;
  ::i18n::addressinput::GetFormattedNationalAddressLine(*address_data,
                                                        &address_line);
  return base::UTF8ToUTF16(address_line);
}

base::string16 GetLabelStreetAddress(const std::vector<ServerFieldType>& types,
                                     const AutofillProfile& profile,
                                     const std::string& app_locale) {
  std::unique_ptr<::i18n::addressinput::AddressData> address_data =
      i18n::CreateAddressDataFromAutofillProfile(
          MakeTrimmedProfile(profile, app_locale, types), app_locale);

  std::string address_line;
  ::i18n::addressinput::GetStreetAddressLinesAsSingleLine(*address_data,
                                                          &address_line);
  return base::UTF8ToUTF16(address_line);
}

base::string16 GetLabelForProfileOnFocusedNonStreetAddress(
    bool form_has_street_address,
    const AutofillProfile& profile,
    const std::string& app_locale,
    const std::vector<ServerFieldType>& types,
    const base::string16& contact_info) {
  std::vector<base::string16> label_parts;
  AddLabelPartIfNotEmpty(
      GetLabelAddress(form_has_street_address, profile, app_locale, types),
      &label_parts);
  AddLabelPartIfNotEmpty(contact_info, &label_parts);
  return ConstructLabelLine(label_parts);
}

base::string16 GetLabelName(const std::vector<ServerFieldType>& types,
                            const AutofillProfile& profile,
                            const std::string& app_locale) {
  bool has_first_name = false;
  bool has_last_name = false;
  bool has_full_name = false;

  for (const ServerFieldType type : types) {
    if (type == NAME_FULL) {
      has_full_name = true;
      break;
    }
    if (type == NAME_FIRST) {
      has_first_name = true;
    }
    if (type == NAME_LAST) {
      has_last_name = true;
    }
  }

  if (has_full_name) {
    return profile.GetInfo(AutofillType(NAME_FULL), app_locale);
  }

  if (has_first_name && has_last_name) {
    std::vector<base::string16> name_parts;
    AddLabelPartIfNotEmpty(GetLabelFirstName(profile, app_locale), &name_parts);
    AddLabelPartIfNotEmpty(profile.GetInfo(AutofillType(NAME_LAST), app_locale),
                           &name_parts);
    return base::JoinString(name_parts, base::ASCIIToUTF16(" "));
  }

  if (has_first_name) {
    return GetLabelFirstName(profile, app_locale);
  }

  if (has_last_name) {
    return profile.GetInfo(AutofillType(NAME_LAST), app_locale);
  }

  // The form contains neither a full name field nor a first name field,
  // so choose some name field in the form and make it the label text.
  for (const ServerFieldType type : types) {
    if (AutofillType(AutofillType(type).GetStorableType()).group() == NAME) {
      return profile.GetInfo(AutofillType(type), app_locale);
    }
  }
  return base::string16();
}

base::string16 GetLabelFirstName(const AutofillProfile& profile,
                                 const std::string& app_locale) {
  return profile.GetInfo(AutofillType(NAME_FIRST), app_locale);
}

base::string16 GetLabelEmail(const AutofillProfile& profile,
                             const std::string& app_locale) {
  const base::string16 email =
      profile.GetInfo(AutofillType(EMAIL_ADDRESS), app_locale);
  return IsValidEmailAddress(email) ? email : base::string16();
}

base::string16 GetLabelPhone(const AutofillProfile& profile,
                             const std::string& app_locale) {
  const std::string unformatted_phone = base::UTF16ToUTF8(
      profile.GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), app_locale));
  return unformatted_phone.empty()
             ? base::string16()
             : base::UTF8ToUTF16(i18n::FormatPhoneNationallyForDisplay(
                   unformatted_phone,
                   data_util::GetCountryCodeWithFallback(profile, app_locale)));
}

bool HaveSameEmailAddresses(const std::vector<AutofillProfile*>& profiles,
                            const std::string& app_locale) {
  return HaveSameData(profiles, app_locale, base::BindRepeating(&GetLabelEmail),
                      base::BindRepeating(base::BindRepeating(&Equals)));
}

bool HaveSameFirstNames(const std::vector<AutofillProfile*>& profiles,
                        const std::string& app_locale) {
  return HaveSameData(profiles, app_locale,
                      base::BindRepeating(&GetLabelFirstName),
                      base::BindRepeating(base::BindRepeating(&Equals)));
}

bool HaveSameNonStreetAddresses(const std::vector<AutofillProfile*>& profiles,
                                const std::string& app_locale,
                                const std::vector<ServerFieldType>& types) {
  // In general, comparing non street addresses with Equals, which uses ==, is
  // not ideal since Düsseldorf and Dusseldorf will be considered distinct. It's
  // okay to use it here because near-duplicate non street addresses like this
  // are filtered out before a LabelFormatter is created.
  return HaveSameData(profiles, app_locale,
                      base::BindRepeating(&GetLabelNationalAddress, types),
                      base::BindRepeating(&Equals));
}

bool HaveSamePhoneNumbers(const std::vector<AutofillProfile*>& profiles,
                          const std::string& app_locale) {
  // Note that the same country code is used in all comparisons.
  auto equals = [](const std::string& country_code,
                   const std::string& app_locale, const base::string16& phone1,
                   const base::string16& phone2) -> bool {
    return (phone1.empty() && phone2.empty()) ||
           i18n::PhoneNumbersMatch(phone1, phone2, country_code, app_locale);
  };

  return profiles.size() <= 1
             ? true
             : HaveSameData(
                   profiles, app_locale, base::BindRepeating(&GetLabelPhone),
                   base::BindRepeating(equals,
                                       base::UTF16ToASCII(profiles[0]->GetInfo(
                                           ADDRESS_HOME_COUNTRY, app_locale)),
                                       app_locale));
}

bool HaveSameStreetAddresses(const std::vector<AutofillProfile*>& profiles,
                             const std::string& app_locale,
                             const std::vector<ServerFieldType>& types) {
  // In general, comparing street addresses with Equals, which uses ==, is not
  // ideal since 3 Elm St and 3 Elm St. will be considered distinct. It's okay
  // to use it here because near-duplicate addresses like this are filtered
  // out before a LabelFormatter is created.
  return HaveSameData(profiles, app_locale,
                      base::BindRepeating(&GetLabelStreetAddress, types),
                      base::BindRepeating(&Equals));
}

bool HasUnfocusedEmailField(FieldTypeGroup focused_group,
                            uint32_t form_groups) {
  return ContainsEmail(form_groups) && focused_group != EMAIL;
}

bool HasUnfocusedNameField(FieldTypeGroup focused_group, uint32_t form_groups) {
  return ContainsName(form_groups) && focused_group != NAME;
}

bool HasUnfocusedNonStreetAddressField(
    ServerFieldType focused_field,
    FieldTypeGroup focused_group,
    const std::vector<ServerFieldType>& types) {
  return HasNonStreetAddress(types) && (focused_group != ADDRESS_HOME ||
                                        !IsNonStreetAddressPart(focused_field));
}

bool HasUnfocusedPhoneField(FieldTypeGroup focused_group,
                            uint32_t form_groups) {
  return ContainsPhone(form_groups) && focused_group != PHONE_HOME;
}

bool HasUnfocusedStreetAddressField(ServerFieldType focused_field,
                                    FieldTypeGroup focused_group,
                                    const std::vector<ServerFieldType>& types) {
  return HasStreetAddress(types) &&
         (focused_group != ADDRESS_HOME || !IsStreetAddressPart(focused_field));
}

bool FormHasOnlyNonStreetAddressFields(
    const std::vector<ServerFieldType>& types,
    uint32_t form_groups) {
  return ContainsAddress(form_groups) && !HasStreetAddress(types) &&
         !(ContainsName(form_groups) || ContainsPhone(form_groups) ||
           ContainsEmail(form_groups));
}

}  // namespace autofill
