// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/address.h"

#include <stddef.h>
#include <algorithm>
#include <memory>

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

Address::Address() : structured_address_(std::make_unique<AddressNode>()){};

Address::Address(const Address& address) {
  *this = address;
}

Address::~Address() = default;

Address& Address::operator=(const Address& address) {
  if (this == &address) {
    return *this;
  }
  structured_address_->CopyFrom(address.GetStructuredAddress());
  return *this;
};

bool Address::operator==(const Address& other) const {
  if (this == &other)
    return true;
  return structured_address_->SameAs(*other.structured_address_.get());
}

bool Address::FinalizeAfterImport() {
  structured_address_->MigrateLegacyStructure();
  bool result = structured_address_->CompleteFullTree();
  // If the address could not be completed, it is possible that it contains an
  // invalid structure.
  if (!result) {
    if (structured_address_->WipeInvalidStructure()) {
      // If the structure was wiped because it is invalid, try to complete the
      // address again.
      result = structured_address_->CompleteFullTree();
    }
  }
  return result;
}

bool Address::MergeStructuredAddress(const Address& newer,
                                     bool newer_was_more_recently_used) {
  return structured_address_->MergeWithComponent(newer.GetStructuredAddress(),
                                                 newer_was_more_recently_used);
}

absl::optional<AlternativeStateNameMap::CanonicalStateName>
Address::GetCanonicalizedStateName() const {
  return AlternativeStateNameMap::GetCanonicalStateName(
      base::UTF16ToUTF8(GetRawInfo(ADDRESS_HOME_COUNTRY)),
      GetRawInfo(ADDRESS_HOME_STATE));
}

bool Address::IsStructuredAddressMergeable(const Address& newer) const {
  return structured_address_->IsMergeableWithComponent(
      newer.GetStructuredAddress());
}

const AddressComponent& Address::GetStructuredAddress() const {
  return *structured_address_.get();
}

std::u16string Address::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(FieldTypeGroup::kAddress, AutofillType(type).group());

  return structured_address_->GetValueForType(type);
}

void Address::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                               const std::u16string& value,
                                               VerificationStatus status) {
  DCHECK_EQ(FieldTypeGroup::kAddress, AutofillType(type).group());
  // The street address has a structure that may have already been set before
  // using the settings dialog. In case the settings dialog was used to change
  // the address to contain different tokens, the structure must be reset.
  if (type == ADDRESS_HOME_STREET_ADDRESS) {
    const std::u16string current_value =
        structured_address_->GetValueForType(type);
    if (!current_value.empty()) {
      AreStringTokenEquivalent(value,
                               structured_address_->GetValueForType(type))
          ? structured_address_->SetValueForType(ADDRESS_HOME_STREET_ADDRESS,
                                                 value, status)
          : structured_address_->SetValueForTypeAndResetSubstructure(
                ADDRESS_HOME_STREET_ADDRESS, value, status);
      return;
    }
  }

  structured_address_->SetValueForType(type, value, status);
}

void Address::GetMatchingTypes(const std::u16string& text,
                               const std::string& app_locale,
                               ServerFieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  std::string country_code = base::UTF16ToUTF8(
      structured_address_->GetValueForType(ADDRESS_HOME_COUNTRY));

  // Check to see if the |text| canonicalized as a country name is a match.
  std::string entered_country_code =
      CountryNames::GetInstance()->GetCountryCodeForLocalizedCountryName(
          text, app_locale);
  if (!entered_country_code.empty() && country_code == entered_country_code)
    matching_types->insert(ADDRESS_HOME_COUNTRY);

  l10n::CaseInsensitiveCompare compare;
  // Check to see if the |text| could be the full name or abbreviation of a
  // state.
  std::u16string canon_text =
      AutofillProfileComparator::NormalizeForComparison(text);
  std::u16string state_name;
  std::u16string state_abbreviation;
  state_names::GetNameAndAbbreviation(canon_text, &state_name,
                                      &state_abbreviation);

  if (!state_name.empty() || !state_abbreviation.empty()) {
    std::u16string canon_profile_state =
        AutofillProfileComparator::NormalizeForComparison(
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
  structured_address_->GetSupportedTypes(supported_types);
}

std::u16string Address::GetInfoImpl(const AutofillType& type,
                                    const std::string& locale) const {
  std::string country_code = base::UTF16ToUTF8(
      structured_address_->GetValueForType(ADDRESS_HOME_COUNTRY));

  if (type.html_type() == HtmlFieldType::kCountryCode) {
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
  if (type.html_type() == HtmlFieldType::kCountryCode) {
    std::string country_code =
        base::IsStringASCII(value)
            ? base::ToUpperASCII(base::UTF16ToASCII(value))
            : std::string();
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

    structured_address_->SetValueForType(
        ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code), status);
    return !country_code.empty();
  }

  if (type.html_type() == HtmlFieldType::kFullAddress) {
    // Parsing a full address is too hard.
    return false;
  }

  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !value.empty()) {
    std::string country_code =
        CountryNames::GetInstance()->GetCountryCodeForLocalizedCountryName(
            value, locale);

    structured_address_->SetValueForType(
        ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code), status);
    return !GetRawInfo(ADDRESS_HOME_COUNTRY).empty();
  }

  SetRawInfoWithVerificationStatus(storable_type, value, status);

  // Give up when importing addresses with any entirely blank lines.
  // There's a good chance that this formatting is not intentional, but it's
  // also not obviously safe to just strip the newlines.
  if (storable_type == ADDRESS_HOME_STREET_ADDRESS) {
    return structured_address_->IsValueForTypeValid(ADDRESS_HOME_STREET_ADDRESS,
                                                    /*wipe_if_not=*/true);
  }

  return true;
}

VerificationStatus Address::GetVerificationStatusImpl(
    ServerFieldType type) const {
  return structured_address_->GetVerificationStatusForType(type);
}

}  // namespace autofill
