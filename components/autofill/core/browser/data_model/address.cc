// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/address.h"

#include <stddef.h>
#include <algorithm>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

using structured_address::VerificationStatus;

Address::Address() = default;

Address::Address(const Address& address) = default;

Address::~Address() = default;

Address& Address::operator=(const Address& address) = default;

bool Address::operator==(const Address& other) const {
  if (this == &other)
    return true;
  // TODO(crbug.com/1130194): Clean legacy implementation once structured
  // addresses are fully launched.
  if (structured_address::StructuredAddressesEnabled()) {
    return structured_address_ == other.structured_address_;
  }

  return street_address_ == other.street_address_ &&
         dependent_locality_ == other.dependent_locality_ &&
         city_ == other.city_ && state_ == other.state_ &&
         zip_code_ == other.zip_code_ && sorting_code_ == other.sorting_code_ &&
         country_code_ == other.country_code_ &&
         street_name_ == other.street_name_ &&
         dependent_street_name_ == other.dependent_street_name_ &&
         house_number_ == other.house_number_ &&
         premise_name_ == other.premise_name_ &&
         subpremise_ == other.subpremise_;
}

bool Address::FinalizeAfterImport(bool profile_is_verified) {
  // TODO(crbug.com/1130194): Remove feature check once structured addresses are
  // fully launched.
  if (structured_address::StructuredAddressesEnabled()) {
    structured_address_.MigrateLegacyStructure(profile_is_verified);
    return structured_address_.CompleteFullTree();
  }
  return true;
}

bool Address::MergeStructuredAddress(const Address& newer,
                                     bool newer_was_more_recently_used) {
  return structured_address_.MergeWithComponent(newer.GetStructuredAddress(),
                                                newer_was_more_recently_used);
}

bool Address::IsStructuredAddressMergeable(const Address& newer) const {
  return structured_address_.IsMergeableWithComponent(
      newer.GetStructuredAddress());
}

const structured_address::Address& Address::GetStructuredAddress() const {
  return structured_address_;
}

base::string16 Address::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(ADDRESS_HOME, AutofillType(type).group());

  // For structured addresses, the value can be directly retrieved.
  if (structured_address::StructuredAddressesEnabled())
    return structured_address_.GetValueForType(type);

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

    // The following tokens are used for creating new type votes but should not
    // be filled into fields.
    case ADDRESS_HOME_STREET_NAME:
      return street_name_;

    case ADDRESS_HOME_HOUSE_NUMBER:
      return house_number_;

    case ADDRESS_HOME_DEPENDENT_STREET_NAME:
      return dependent_street_name_;

    case ADDRESS_HOME_PREMISE_NAME:
      return premise_name_;

    case ADDRESS_HOME_SUBPREMISE:
      return subpremise_;

    default:
      NOTREACHED() << "Unrecognized type: " << type;
      return base::string16();
  }
}

void Address::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                               const base::string16& value,
                                               VerificationStatus status) {
  DCHECK_EQ(ADDRESS_HOME, AutofillType(type).group());

  // For structured addresses, the value can directly be set.
  // TODO(crbug.com/1130194): Clean legacy implementation once structured
  // addresses are fully launched.
  if (structured_address::StructuredAddressesEnabled()) {
    structured_address_.SetValueForTypeIfPossible(type, value, status);
    return;
  }

  switch (type) {
      // If any of the address lines change, the structured tokens must be
      // reset.
    case ADDRESS_HOME_LINE1:
      if (street_address_.empty())
        street_address_.resize(1);
      if (street_address_[0] != value)
        ResetStructuredTokes();
      street_address_[0] = value;
      TrimStreetAddress();
      break;

    case ADDRESS_HOME_LINE2:
      if (street_address_.size() < 2)
        street_address_.resize(2);
      if (street_address_[1] != value)
        ResetStructuredTokes();
      street_address_[1] = value;
      TrimStreetAddress();
      break;

    case ADDRESS_HOME_LINE3:
      if (street_address_.size() < 3)
        street_address_.resize(3);
      if (street_address_[2] != value)
        ResetStructuredTokes();
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
      // If the street address changes, the structured tokens must be reset.
      if (base::SplitString(value, base::ASCIIToUTF16("\n"),
                            base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_ALL) != street_address_) {
        ResetStructuredTokes();
        street_address_ =
            base::SplitString(value, base::ASCIIToUTF16("\n"),
                              base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      }
      break;

    // The following types are used to create type votes but should not be
    // filled into fields.
    case ADDRESS_HOME_STREET_NAME:
      street_name_ = value;
      break;

    case ADDRESS_HOME_DEPENDENT_STREET_NAME:
      dependent_street_name_ = value;
      break;

    case ADDRESS_HOME_HOUSE_NUMBER:
      house_number_ = value;
      break;

    case ADDRESS_HOME_PREMISE_NAME:
      premise_name_ = value;
      break;

    case ADDRESS_HOME_SUBPREMISE:
      subpremise_ = value;
      break;

    default:
      NOTREACHED();
  }
}

void Address::ResetStructuredTokes() {
  street_name_.clear();
  dependent_street_name_.clear();
  house_number_.clear();
  premise_name_.clear();
  subpremise_.clear();
}

void Address::GetMatchingTypes(const base::string16& text,
                               const std::string& app_locale,
                               ServerFieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  // Get the country code stored in the profile either from the structured
  // address if enabled or from the legacy field.
  // TODO(crbug.com/1130194): Clean legacy implementation once structured
  // addresses are fully launched.
  std::string country_code =
      structured_address::StructuredAddressesEnabled()
          ? base::UTF16ToUTF8(
                structured_address_.GetValueForType(ADDRESS_HOME_COUNTRY))
          : country_code_;

  // Check to see if the |text| canonicalized as a country name is a match.
  std::string entered_country_code =
      CountryNames::GetInstance()->GetCountryCodeForLocalizedCountryName(
          text, app_locale);
  if (!entered_country_code.empty() && country_code == entered_country_code)
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
  // If those types are not added, no votes will be generated.
  if (base::FeatureList::IsEnabled(
          features::kAutofillAddressEnhancementVotes) ||
      structured_address::StructuredAddressesEnabled()) {
    supported_types->insert(ADDRESS_HOME_STREET_NAME);
    supported_types->insert(ADDRESS_HOME_DEPENDENT_STREET_NAME);
    supported_types->insert(ADDRESS_HOME_HOUSE_NUMBER);
    supported_types->insert(ADDRESS_HOME_PREMISE_NAME);
    supported_types->insert(ADDRESS_HOME_SUBPREMISE);
  }
}

base::string16 Address::GetInfoImpl(const AutofillType& type,
                                    const std::string& locale) const {
  // Get the country code stored in the profile either from the structured
  // address if enabled or from the legacy field.
  // TODO(crbug.com/1130194): Clean legacy implementation once structured
  // addresses are fully launched.
  std::string country_code =
      structured_address::StructuredAddressesEnabled()
          ? base::UTF16ToUTF8(
                structured_address_.GetValueForType(ADDRESS_HOME_COUNTRY))
          : country_code_;

  if (type.html_type() == HTML_TYPE_COUNTRY_CODE) {
    return base::ASCIIToUTF16(country_code);
  }

  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !country_code.empty())
    return AutofillCountry(country_code, locale).name();

  return GetRawInfo(storable_type);
}

bool Address::SetInfoWithVerificationStatusImpl(const AutofillType& type,
                                                const base::string16& value,
                                                const std::string& locale,
                                                VerificationStatus status) {
  // TODO(crbug.com/1130194): Clean legacy implementation once structured
  // addresses are fully launched.
  bool use_structured_address =
      structured_address::StructuredAddressesEnabled();

  if (type.html_type() == HTML_TYPE_COUNTRY_CODE) {
    std::string country_code = base::ToUpperASCII(base::UTF16ToASCII(value));
    if (!data_util::IsValidCountryCode(country_code)) {
      // Some popular websites use the HTML_TYPE_COUNTRY_CODE attribute for
      // full text names (e.g. alliedelec.com). Try to convert the value to a
      // country code as a fallback.
      if (base::FeatureList::IsEnabled(
              features::kAutofillAllowHtmlTypeCountryCodesWithFullNames)) {
        CountryNames* country_names =
            !value.empty() ? CountryNames::GetInstance() : nullptr;
        country_code =
            country_names
                ? country_names->GetCountryCodeForLocalizedCountryName(value,
                                                                       locale)
                : std::string();
      } else {
        country_code = std::string();
      }
    }

    // TODO(crbug.com/1130194): Clean legacy implementation once structured
    // addresses are fully launched.
    if (use_structured_address) {
      structured_address_.SetValueForTypeIfPossible(ADDRESS_HOME_COUNTRY,
                                                    country_code, status);
    } else {
      country_code_ = country_code;
    }
    return !country_code.empty();
  }

  if (type.html_type() == HTML_TYPE_FULL_ADDRESS) {
    // Parsing a full address is too hard.
    return false;
  }

  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !value.empty()) {
    std::string country_code =
        CountryNames::GetInstance()->GetCountryCodeForLocalizedCountryName(
            value, locale);
    // TODO(crbug.com/1130194): Clean legacy implementation once structured
    // addresses are fully launched.
    if (use_structured_address) {
      structured_address_.SetValueForTypeIfPossible(ADDRESS_HOME_COUNTRY,
                                                    country_code, status);
    } else {
      country_code_ = country_code;
    }
    return !country_code_.empty();
  }

  SetRawInfoWithVerificationStatus(storable_type, value, status);

  // Give up when importing addresses with any entirely blank lines.
  // There's a good chance that this formatting is not intentional, but it's
  // also not obviously safe to just strip the newlines.
  if (storable_type == ADDRESS_HOME_STREET_ADDRESS) {
    // TODO(crbug.com/1130194): Clean legacy implementation once structured
    // addresses are fully launched.
    if (structured_address::StructuredAddressesEnabled()) {
      return structured_address_.IsValueForTypeValid(
          ADDRESS_HOME_STREET_ADDRESS, /*wipe_if_not=*/true);
    } else if (base::Contains(street_address_, base::string16())) {
      street_address_.clear();
      return false;
    }
  }

  return true;
}

VerificationStatus Address::GetVerificationStatusImpl(
    ServerFieldType type) const {
  // TODO(crbug.com/1130194): Clean legacy implementation once structured
  // addresses are fully launched.
  if (structured_address::StructuredAddressesEnabled())
    return structured_address_.GetVerificationStatusForType(type);
  return VerificationStatus::kNoStatus;
}

void Address::TrimStreetAddress() {
  while (!street_address_.empty() && street_address_.back().empty()) {
    street_address_.pop_back();
  }
}

}  // namespace autofill
