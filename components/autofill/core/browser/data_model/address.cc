// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/address.h"

#include <stddef.h>
#include <algorithm>

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

Address::Address() {}

Address::Address(const Address& address) {
  *this = address;
}

Address::~Address() {}

Address& Address::operator=(const Address& address) {
  if (this == &address)
    return *this;

  street_address_ = address.street_address_;
  dependent_locality_ = address.dependent_locality_;
  city_ = address.city_;
  state_ = address.state_;
  country_code_ = address.country_code_;
  zip_code_ = address.zip_code_;
  sorting_code_ = address.sorting_code_;
  return *this;
}

bool Address::operator==(const Address& other) const {
  if (this == &other)
    return true;
  return street_address_ == other.street_address_ &&
         dependent_locality_ == other.dependent_locality_ &&
         city_ == other.city_ && state_ == other.state_ &&
         zip_code_ == other.zip_code_ && sorting_code_ == other.sorting_code_ &&
         country_code_ == other.country_code_;
}

base::string16 Address::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(ADDRESS_HOME, AutofillType(type).group());
  switch (type) {
    case ADDRESS_HOME_LINE1:
      return street_address_.size() > 0 ? street_address_[0] : base::string16();

    case ADDRESS_HOME_LINE2:
      return street_address_.size() > 1 ? street_address_[1] : base::string16();

    case ADDRESS_HOME_LINE3:
      return street_address_.size() > 2 ? street_address_[2] : base::string16();

    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      return dependent_locality_;

    case ADDRESS_HOME_CITY:
      return city_;

    case ADDRESS_HOME_STATE:
      return state_;

    case ADDRESS_HOME_ZIP:
      return zip_code_;

    case ADDRESS_HOME_SORTING_CODE:
      return sorting_code_;

    case ADDRESS_HOME_COUNTRY:
      return base::ASCIIToUTF16(country_code_);

    case ADDRESS_HOME_STREET_ADDRESS:
      return base::JoinString(street_address_, base::ASCIIToUTF16("\n"));

    case ADDRESS_HOME_APT_NUM:
      return base::string16();

    default:
      NOTREACHED() << "Unrecognized type: " << type;
      return base::string16();
  }
}

void Address::SetRawInfo(ServerFieldType type, const base::string16& value) {
  DCHECK_EQ(ADDRESS_HOME, AutofillType(type).group());
  switch (type) {
    case ADDRESS_HOME_LINE1:
      if (street_address_.empty())
        street_address_.resize(1);
      street_address_[0] = value;
      TrimStreetAddress();
      break;

    case ADDRESS_HOME_LINE2:
      if (street_address_.size() < 2)
        street_address_.resize(2);
      street_address_[1] = value;
      TrimStreetAddress();
      break;

    case ADDRESS_HOME_LINE3:
      if (street_address_.size() < 3)
        street_address_.resize(3);
      street_address_[2] = value;
      TrimStreetAddress();
      break;

    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      dependent_locality_ = value;
      break;

    case ADDRESS_HOME_CITY:
      city_ = value;
      break;

    case ADDRESS_HOME_STATE:
      state_ = value;
      break;

    case ADDRESS_HOME_COUNTRY:
      DCHECK(value.empty() ||
             data_util::IsValidCountryCode(base::i18n::ToUpper(value)));
      country_code_ = base::ToUpperASCII(base::UTF16ToASCII(value));
      break;

    case ADDRESS_HOME_ZIP:
      zip_code_ = value;
      break;

    case ADDRESS_HOME_SORTING_CODE:
      sorting_code_ = value;
      break;

    case ADDRESS_HOME_STREET_ADDRESS:
      street_address_ =
          base::SplitString(value, base::ASCIIToUTF16("\n"),
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      break;

    default:
      NOTREACHED();
  }
}

void Address::GetMatchingTypes(const base::string16& text,
                               const std::string& app_locale,
                               ServerFieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  // Check to see if the |text| canonicalized as a country name is a match.
  std::string country_code = CountryNames::GetInstance()->GetCountryCode(text);
  if (!country_code.empty() && country_code_ == country_code)
    matching_types->insert(ADDRESS_HOME_COUNTRY);

  AutofillProfileComparator comparator(app_locale);
  // Check to see if the |text| could be the full name or abbreviation of a
  // state.
  base::string16 canon_text = comparator.NormalizeForComparison(text);
  base::string16 state_name;
  base::string16 state_abbreviation;
  state_names::GetNameAndAbbreviation(canon_text, &state_name,
                                      &state_abbreviation);
  if (!state_name.empty() || !state_abbreviation.empty()) {
    l10n::CaseInsensitiveCompare compare;
    base::string16 canon_profile_state = comparator.NormalizeForComparison(
        GetInfo(AutofillType(ADDRESS_HOME_STATE), app_locale));
    if ((!state_name.empty() &&
         compare.StringsEqual(state_name, canon_profile_state)) ||
        (!state_abbreviation.empty() &&
         compare.StringsEqual(state_abbreviation, canon_profile_state))) {
      matching_types->insert(ADDRESS_HOME_STATE);
    }
  }
}

void Address::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(ADDRESS_HOME_LINE1);
  supported_types->insert(ADDRESS_HOME_LINE2);
  supported_types->insert(ADDRESS_HOME_LINE3);
  supported_types->insert(ADDRESS_HOME_STREET_ADDRESS);
  supported_types->insert(ADDRESS_HOME_DEPENDENT_LOCALITY);
  supported_types->insert(ADDRESS_HOME_CITY);
  supported_types->insert(ADDRESS_HOME_STATE);
  supported_types->insert(ADDRESS_HOME_ZIP);
  supported_types->insert(ADDRESS_HOME_SORTING_CODE);
  supported_types->insert(ADDRESS_HOME_COUNTRY);
}

base::string16 Address::GetInfoImpl(const AutofillType& type,
                                    const std::string& app_locale) const {
  if (type.html_type() == HTML_TYPE_COUNTRY_CODE)
    return base::ASCIIToUTF16(country_code_);

  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !country_code_.empty())
    return AutofillCountry(country_code_, app_locale).name();

  return GetRawInfo(storable_type);
}

bool Address::SetInfoImpl(const AutofillType& type,
                          const base::string16& value,
                          const std::string& app_locale) {
  if (type.html_type() == HTML_TYPE_COUNTRY_CODE) {
    if (!data_util::IsValidCountryCode(base::i18n::ToUpper(value))) {
      country_code_ = std::string();
      return false;
    }

    country_code_ = base::ToUpperASCII(base::UTF16ToASCII(value));
    return true;
  }
  if (type.html_type() == HTML_TYPE_FULL_ADDRESS) {
    // Parsing a full address is too hard.
    return false;
  }

  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !value.empty()) {
    country_code_ = CountryNames::GetInstance()->GetCountryCode(value);
    return !country_code_.empty();
  }

  SetRawInfo(storable_type, value);

  // Give up when importing addresses with any entirely blank lines.
  // There's a good chance that this formatting is not intentional, but it's
  // also not obviously safe to just strip the newlines.
  if (storable_type == ADDRESS_HOME_STREET_ADDRESS &&
      base::Contains(street_address_, base::string16())) {
    street_address_.clear();
    return false;
  }

  return true;
}

void Address::TrimStreetAddress() {
  while (!street_address_.empty() && street_address_.back().empty()) {
    street_address_.pop_back();
  }
}

}  // namespace autofill
