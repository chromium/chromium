// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/phone_number.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"

namespace autofill {
namespace {

// Returns the region code for this phone number, which is an ISO 3166 2-letter
// country code.  The returned value is based on the |profile|; if the |profile|
// does not have a country code associated with it, falls back to the country
// code corresponding to the |app_locale|.
std::string GetRegion(const AutofillProfile& profile,
                      const std::string& app_locale) {
  base::string16 country_code = profile.GetRawInfo(ADDRESS_HOME_COUNTRY);
  if (!country_code.empty())
    return base::UTF16ToASCII(country_code);

  return AutofillCountry::CountryCodeForLocale(app_locale);
}

}  // namespace

PhoneNumber::PhoneNumber(const AutofillProfile* profile) : profile_(profile) {}

PhoneNumber::PhoneNumber(const PhoneNumber& number) : profile_(nullptr) {
  *this = number;
}

PhoneNumber::~PhoneNumber() {}

PhoneNumber& PhoneNumber::operator=(const PhoneNumber& number) {
  if (this == &number)
    return *this;

  number_ = number.number_;
  profile_ = number.profile_;
  cached_parsed_phone_ = number.cached_parsed_phone_;
  return *this;
}

bool PhoneNumber::operator==(const PhoneNumber& other) const {
  if (this == &other)
    return true;

  return number_ == other.number_ && profile_ == other.profile_;
}

void PhoneNumber::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(PHONE_HOME_WHOLE_NUMBER);
  supported_types->insert(PHONE_HOME_NUMBER);
  supported_types->insert(PHONE_HOME_CITY_CODE);
  supported_types->insert(PHONE_HOME_CITY_AND_NUMBER);
  supported_types->insert(PHONE_HOME_COUNTRY_CODE);
}

base::string16 PhoneNumber::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(PHONE_HOME, AutofillType(type).group());
  if (type == PHONE_HOME_WHOLE_NUMBER)
    return number_;

  // Only the whole number is available as raw data.  All of the other types are
  // parsed from this raw info, and parsing requires knowledge of the phone
  // number's region, which is only available via GetInfo().
  return base::string16();
}

void PhoneNumber::SetRawInfo(ServerFieldType type,
                             const base::string16& value) {
  DCHECK_EQ(PHONE_HOME, AutofillType(type).group());
  if (type != PHONE_HOME_CITY_AND_NUMBER && type != PHONE_HOME_WHOLE_NUMBER) {
    // Only full phone numbers should be set directly.  The remaining field
    // field types are read-only.
    return;
  }

  number_ = value;

  // Invalidate the cached number.
  cached_parsed_phone_ = i18n::PhoneObject();
}

void PhoneNumber::GetMatchingTypes(const base::string16& text,
                                   const std::string& app_locale,
                                   ServerFieldTypeSet* matching_types) const {
  // Strip the common phone number non numerical characters before calling the
  // base matching type function. For example, the |text| "(514) 121-1523"
  // would become the stripped text "5141211523". Since the base matching
  // function only does simple canonicalization to match against the stored
  // data, some domain specific cases will be covered below.
  base::string16 stripped_text = text;
  base::RemoveChars(stripped_text, base::ASCIIToUTF16(" .()-"), &stripped_text);
  FormGroup::GetMatchingTypes(stripped_text, app_locale, matching_types);

  // For US numbers, also compare to the three-digit prefix and the four-digit
  // suffix, since web sites often split numbers into these two fields.
  base::string16 number = GetInfo(AutofillType(PHONE_HOME_NUMBER), app_locale);
  if (GetRegion(*profile_, app_locale) == "US" &&
      number.size() == (kPrefixLength + kSuffixLength)) {
    base::string16 prefix = number.substr(kPrefixOffset, kPrefixLength);
    base::string16 suffix = number.substr(kSuffixOffset, kSuffixLength);
    if (text == prefix || text == suffix)
      matching_types->insert(PHONE_HOME_NUMBER);
  }

  // TODO(crbug.com/581391): Investigate the use of PhoneNumberUtil when
  // matching phone numbers for upload.
  // If there is not already a match for PHONE_HOME_WHOLE_NUMBER, normalize the
  // |text| based on the app_locale before comparing it to the whole number. For
  // example, the France number "33 2 49 19 70 70" would be normalized to
  // "+33249197070" whereas the US number "+1 (234) 567-8901" would be
  // normalized to "12345678901".
  if (matching_types->find(PHONE_HOME_WHOLE_NUMBER) == matching_types->end()) {
    base::string16 whole_number =
        GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), app_locale);
    if (!whole_number.empty()) {
      base::string16 normalized_number =
          i18n::NormalizePhoneNumber(text, GetRegion(*profile_, app_locale));
      if (normalized_number == whole_number)
        matching_types->insert(PHONE_HOME_WHOLE_NUMBER);
    }
  }
}

// Normalize phones if |type| is a whole number:
//   (650)2345678 -> 6502345678
//   1-800-FLOWERS -> 18003569377
// If the phone cannot be normalized, returns the stored value verbatim.
base::string16 PhoneNumber::GetInfoImpl(const AutofillType& type,
                                        const std::string& app_locale) const {
  ServerFieldType storable_type = type.GetStorableType();
  UpdateCacheIfNeeded(app_locale);

  // When the phone number autofill has stored cannot be normalized, it
  // responds to queries for complete numbers with whatever the raw stored value
  // is, and simply return empty string for any queries for phone components.
  if (!cached_parsed_phone_.IsValidNumber()) {
    if (storable_type == PHONE_HOME_WHOLE_NUMBER ||
        storable_type == PHONE_HOME_CITY_AND_NUMBER) {
      return cached_parsed_phone_.GetWholeNumber();
    }
    return base::string16();
  }

  switch (storable_type) {
    case PHONE_HOME_WHOLE_NUMBER:
      return cached_parsed_phone_.GetWholeNumber();

    case PHONE_HOME_NUMBER:
      return cached_parsed_phone_.number();

    case PHONE_HOME_CITY_CODE:
      return cached_parsed_phone_.city_code();

    case PHONE_HOME_COUNTRY_CODE:
      return cached_parsed_phone_.country_code();

    case PHONE_HOME_CITY_AND_NUMBER: {
      // Just concatenating city code and phone number is insufficient because
      // a number of non-US countries (e.g. Germany and France) use a leading 0
      // to indicate that the next digits represent a city code.
      base::string16 national_number =
          cached_parsed_phone_.GetNationallyFormattedNumber();
      // GetNationallyFormattedNumber optimizes for screen display, e.g. it
      // shows a US number as (888) 123-1234. The following retains only the
      // digits.
      national_number.erase(
          std::remove_if(national_number.begin(), national_number.end(),
                         [](auto c) { return !std::isdigit(c); }),
          national_number.end());
      return national_number;
    }

    case PHONE_HOME_EXTENSION:
      return base::string16();

    default:
      NOTREACHED();
      return base::string16();
  }
}

bool PhoneNumber::SetInfoImpl(const AutofillType& type,
                              const base::string16& value,
                              const std::string& app_locale) {
  SetRawInfo(type.GetStorableType(), value);

  if (number_.empty())
    return true;

  // Store a formatted (i.e., pretty printed) version of the number if either
  // the number doesn't contain formatting marks.
  UpdateCacheIfNeeded(app_locale);
  if (base::ContainsOnlyChars(number_, base::ASCIIToUTF16("+0123456789"))) {
    number_ = cached_parsed_phone_.GetFormattedNumber();
  } else if (i18n::NormalizePhoneNumber(number_,
                                        GetRegion(*profile_, app_locale))
                 .empty()) {
    // The number doesn't make sense for this region; clear it.
    number_.clear();
  }
  return !number_.empty();
}

void PhoneNumber::UpdateCacheIfNeeded(const std::string& app_locale) const {
  std::string region = GetRegion(*profile_, app_locale);
  if (!number_.empty() && cached_parsed_phone_.region() != region)
    cached_parsed_phone_ = i18n::PhoneObject(number_, region);
}

PhoneNumber::PhoneCombineHelper::PhoneCombineHelper() {}

PhoneNumber::PhoneCombineHelper::~PhoneCombineHelper() {}

bool PhoneNumber::PhoneCombineHelper::SetInfo(const AutofillType& type,
                                              const base::string16& value) {
  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == PHONE_HOME_COUNTRY_CODE) {
    country_ = value;
    return true;
  }

  if (storable_type == PHONE_HOME_CITY_CODE) {
    city_ = value;
    return true;
  }

  if (storable_type == PHONE_HOME_CITY_AND_NUMBER) {
    phone_ = value;
    return true;
  }

  if (storable_type == PHONE_HOME_WHOLE_NUMBER) {
    whole_number_ = value;
    return true;
  }

  if (storable_type == PHONE_HOME_NUMBER) {
    phone_.append(value);
    return true;
  }

  return false;
}

bool PhoneNumber::PhoneCombineHelper::ParseNumber(
    const AutofillProfile& profile,
    const std::string& app_locale,
    base::string16* value) {
  if (IsEmpty())
    return false;

  if (!whole_number_.empty()) {
    *value = whole_number_;
    return true;
  }

  return i18n::ConstructPhoneNumber(country_, city_, phone_,
                                    GetRegion(profile, app_locale), value);
}

bool PhoneNumber::PhoneCombineHelper::IsEmpty() const {
  return phone_.empty() && whole_number_.empty();
}

}  // namespace autofill
