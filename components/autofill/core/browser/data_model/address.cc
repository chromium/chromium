// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/address.h"

#include <stddef.h>
#include <algorithm>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
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
    return structured_address_.SameAs(other.structured_address_);
  }

  bool are_states_equal = (state_ == other.state_);
  if (base::FeatureList::IsEnabled(
          features::kAutofillUseAlternativeStateNameMap) &&
      !are_states_equal) {
    // If the canonical state name exists for |state_| and |other.state_|, they
    // are compared otherwise.
    absl::optional<AlternativeStateNameMap::CanonicalStateName>
        canonical_state_name_cur = GetCanonicalizedStateName();
    absl::optional<AlternativeStateNameMap::CanonicalStateName>
        canonical_state_name_other = other.GetCanonicalizedStateName();
    if (canonical_state_name_cur && canonical_state_name_other) {
      are_states_equal =
          (canonical_state_name_cur == canonical_state_name_other);
    }
  }

  return street_address_ == other.street_address_ &&
         dependent_locality_ == other.dependent_locality_ &&
         city_ == other.city_ && zip_code_ == other.zip_code_ &&
         sorting_code_ == other.sorting_code_ &&
         country_code_ == other.country_code_ && are_states_equal &&
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
    bool result = structured_address_.CompleteFullTree();
    // If the address could not be completed, it is possible that it contains an
    // invalid structure.
    if (!result) {
      if (structured_address_.WipeInvalidStructure()) {
        // If the structure was wiped because it is invalid, try to complete the
        // address again.
        result = structured_address_.CompleteFullTree();
      }
    }
    return result;
  }
  return true;
}

bool Address::MergeStructuredAddress(const Address& newer,
                                     bool newer_was_more_recently_used) {
  return structured_address_.MergeWithComponent(newer.GetStructuredAddress(),
                                                newer_was_more_recently_used);
}

absl::optional<AlternativeStateNameMap::CanonicalStateName>
Address::GetCanonicalizedStateName() const {
  return AlternativeStateNameMap::GetCanonicalStateName(
      base::UTF16ToUTF8(GetRawInfo(ADDRESS_HOME_COUNTRY)),
      GetRawInfo(ADDRESS_HOME_STATE));
}

bool Address::IsStructuredAddressMergeable(const Address& newer) const {
  return structured_address_.IsMergeableWithComponent(
      newer.GetStructuredAddress());
}

const structured_address::Address& Address::GetStructuredAddress() const {
  return structured_address_;
}

std::u16string Address::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(FieldTypeGroup::kAddressHome, AutofillType(type).group());

  // For structured addresses, the value can be directly retrieved.
  if (structured_address::StructuredAddressesEnabled())
    return structured_address_.GetValueForType(type);

  switch (type) {
    case ADDRESS_HOME_LINE1:
      return street_address_.size() > 0 ? street_address_[0] : std::u16string();

    case ADDRESS_HOME_LINE2:
      return street_address_.size() > 1 ? street_address_[1] : std::u16string();

    case ADDRESS_HOME_LINE3:
      return street_address_.size() > 2 ? street_address_[2] : std::u16string();

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
      return base::JoinString(street_address_, u"\n");

    case ADDRESS_HOME_APT_NUM:
      return std::u16string();

    case ADDRESS_HOME_FLOOR:
      return std::u16string();

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

    case ADDRESS_HOME_ADDRESS:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
      return std::u16string();

    default:
      NOTREACHED() << "Unrecognized type: " << type;
      return std::u16string();
  }
}

void Address::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                               const std::u16string& value,
                                               VerificationStatus status) {
  DCHECK_EQ(FieldTypeGroup::kAddressHome, AutofillType(type).group());

  // For structured addresses, the value can directly be set.
  // TODO(crbug.com/1130194): Clean legacy implementation once structured
  // addresses are fully launched.
  if (structured_address::StructuredAddressesEnabled()) {
    // The street address has a structure that may have already been set before
    // using the settings dialog. In case the settings dialog was used to change
    // the address to contain different tokens, the structure must be reset.
    if (type == ADDRESS_HOME_STREET_ADDRESS) {
      const std::u16string current_value =
          structured_address_.GetValueForType(type);
      if (!current_value.empty()) {
        bool token_equivalent = structured_address::AreStringTokenEquivalent(
            value, structured_address_.GetValueForType(type));
        structured_address_.SetValueForTypeIfPossible(
            ADDRESS_HOME_STREET_ADDRESS, value, status,
            /*invalidate_child_nodes=*/!token_equivalent);
        return;
      }
    }

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
      if (base::SplitString(value, u"\n", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_ALL) != street_address_) {
        ResetStructuredTokes();
        street_address_ = base::SplitString(value, u"\n", base::TRIM_WHITESPACE,
                                            base::SPLIT_WANT_ALL);
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

    // Not implemented for unstructured addresses.
    case ADDRESS_HOME_APT_NUM:
      break;

    // Not implemented for unstructured addresses.
    case ADDRESS_HOME_FLOOR:
      break;

    case ADDRESS_HOME_ADDRESS:
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

void Address::GetMatchingTypes(const std::u16string& text,
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

  l10n::CaseInsensitiveCompare compare;
  AutofillProfileComparator comparator(app_locale);
  // Check to see if the |text| could be the full name or abbreviation of a
  // state.
  std::u16string canon_text = comparator.NormalizeForComparison(text);
  std::u16string state_name;
  std::u16string state_abbreviation;
  state_names::GetNameAndAbbreviation(canon_text, &state_name,
                                      &state_abbreviation);

  if (!state_name.empty() || !state_abbreviation.empty()) {
    std::u16string canon_profile_state = comparator.NormalizeForComparison(
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
  if (structured_address::StructuredAddressesEnabled()) {
    supported_types->insert(ADDRESS_HOME_STREET_NAME);
    supported_types->insert(ADDRESS_HOME_DEPENDENT_STREET_NAME);
    supported_types->insert(ADDRESS_HOME_HOUSE_NUMBER);
    supported_types->insert(ADDRESS_HOME_PREMISE_NAME);
    supported_types->insert(ADDRESS_HOME_SUBPREMISE);
  }
}

std::u16string Address::GetInfoImpl(const AutofillType& type,
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
                                                const std::u16string& value,
                                                const std::string& locale,
                                                VerificationStatus status) {
  // TODO(crbug.com/1130194): Clean legacy implementation once structured
  // addresses are fully launched.
  bool use_structured_address =
      structured_address::StructuredAddressesEnabled();

  if (type.html_type() == HTML_TYPE_COUNTRY_CODE) {
    std::string country_code = base::ToUpperASCII(base::UTF16ToASCII(value));
    if (!data_util::IsValidCountryCode(country_code)) {
      // To counteract the misuse of autocomplete=country attribute when used
      // with full country names, if the supplied country code is not a valid,
      // it is tested if a country code can be derived from the value when it is
      // interpreted as a full country name. Otherwise an empty string is
      // assigned to |country_code|.
      CountryNames* country_names =
          !value.empty() ? CountryNames::GetInstance() : nullptr;
      country_code = country_names
                         ? country_names->GetCountryCodeForLocalizedCountryName(
                               value, locale)
                         : std::string();
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
    return !GetRawInfo(ADDRESS_HOME_COUNTRY).empty();
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
    } else if (base::Contains(street_address_, std::u16string())) {
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
