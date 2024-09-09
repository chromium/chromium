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
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

Address::Address(AddressCountryCode country_code)
    : address_component_store_(
          i18n_model_definition::CreateAddressComponentModel(country_code)),
      is_legacy_address_(
          !i18n_model_definition::IsCustomHierarchyAvailableForCountry(
              country_code)) {}

Address::~Address() = default;

Address::Address(const Address& address) {
  *this = address;
}

Address& Address::operator=(const Address& address) {
  if (this == &address) {
    return *this;
  }

  address_component_store_ = i18n_model_definition::CreateAddressComponentModel(
      address.IsLegacyAddress() ? AddressCountryCode("")
                                : address.GetAddressCountryCode());
  Root()->CopyFrom(address.GetRoot());
  is_legacy_address_ = address.IsLegacyAddress();
  return *this;
}

bool Address::operator==(const Address& other) const {
  if (this == &other)
    return true;
  return GetRoot().SameAs(other.GetRoot());
}

bool Address::FinalizeAfterImport() {
  Root()->MigrateLegacyStructure();
  bool result = Root()->CompleteFullTree();
  // If the address could not be completed, it is possible that it contains an
  // invalid structure.
  if (!result) {
    if (Root()->WipeInvalidStructure()) {
      // If the structure was wiped because it is invalid, try to complete the
      // address again.
      result = Root()->CompleteFullTree();
    }
  }
  // Generate synthesized node values after the tree's values were updated,
  // whether by completion or gaps filled.
  Root()->GenerateTreeSynthesizedNodes();
  return result;
}

bool Address::MergeStructuredAddress(const Address& newer,
                                     bool newer_was_more_recently_used) {
  return Root()->MergeWithComponent(newer.GetRoot(),
                                    newer_was_more_recently_used);
}

std::optional<AlternativeStateNameMap::CanonicalStateName>
Address::GetCanonicalizedStateName() const {
  return AlternativeStateNameMap::GetCanonicalStateName(
      GetAddressCountryCode().value(), GetRawInfo(ADDRESS_HOME_STATE));
}

bool Address::IsStructuredAddressMergeable(const Address& newer) const {
  return GetRoot().IsMergeableWithComponent(newer.GetRoot());
}

bool Address::IsStructuredAddressMergeableForType(FieldType type,
                                                  const Address& other) const {
  return address_component_store_.GetNodeForType(type)
      ->IsMergeableWithComponent(
          *other.address_component_store_.GetNodeForType(type));
}

const AddressComponent& Address::GetRoot() const {
  return *address_component_store_.Root();
}

AddressComponent* Address::Root() {
  return address_component_store_.Root();
}

AddressCountryCode Address::GetAddressCountryCode() const {
  return GetRoot().GetCountryCode();
}

std::u16string Address::GetRawInfo(FieldType type) const {
  DCHECK_EQ(FieldTypeGroup::kAddress, GroupTypeOfFieldType(type));

  return GetRoot().GetValueForType(type);
}

void Address::SetRawInfoWithVerificationStatus(FieldType type,
                                               const std::u16string& value,
                                               VerificationStatus status) {
  DCHECK_EQ(FieldTypeGroup::kAddress, GroupTypeOfFieldType(type));
  // The street address has a structure that may have already been set before
  // using the settings dialog. In case the settings dialog was used to change
  // the address to contain different tokens, the structure must be reset.
  if (type == ADDRESS_HOME_STREET_ADDRESS) {
    const std::u16string current_value = Root()->GetValueForType(type);
    if (!current_value.empty()) {
      AreStringTokenEquivalent(value, Root()->GetValueForType(type))
          ? Root()->SetValueForType(ADDRESS_HOME_STREET_ADDRESS, value, status)
          : Root()->SetValueForTypeAndResetSubstructure(
                ADDRESS_HOME_STREET_ADDRESS, value, status);
      return;
    }
  }

  if (type == ADDRESS_HOME_COUNTRY) {
    SetAddressCountryCode(value, status);
    return;
  }

  Root()->SetValueForType(type, value, status);
}

void Address::GetMatchingTypesWithProfileSources(
    const std::u16string& text,
    const std::string& app_locale,
    FieldTypeSet* matching_types,
    PossibleProfileValueSources* profile_value_sources) const {
  FormGroup::GetMatchingTypesWithProfileSources(
      text, app_locale, matching_types, profile_value_sources);

  std::string country_code = GetRoot().GetCountryCode().value();

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
            GetInfo(ADDRESS_HOME_STATE, app_locale));
    if ((!state_name.empty() &&
         compare.StringsEqual(state_name, canon_profile_state)) ||
        (!state_abbreviation.empty() &&
         compare.StringsEqual(state_abbreviation, canon_profile_state))) {
      matching_types->insert(ADDRESS_HOME_STATE);
    }
  }
}

void Address::GetSupportedTypes(FieldTypeSet* supported_types) const {
  GetRoot().GetSupportedTypes(supported_types);
}

std::u16string Address::GetInfoImpl(const AutofillType& type,
                                    const std::string& locale) const {
  std::string country_code =
      base::UTF16ToUTF8(GetRoot().GetValueForType(ADDRESS_HOME_COUNTRY));

  if (type.html_type() == HtmlFieldType::kCountryCode) {
    return base::ASCIIToUTF16(country_code);
  }

  FieldType storable_type = type.GetStorableType();
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

    SetRawInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY,
                                     base::UTF8ToUTF16(country_code), status);
    return !country_code.empty();
  }

  FieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !value.empty()) {
    std::string country_code =
        CountryNames::GetInstance()->GetCountryCodeForLocalizedCountryName(
            value, locale);

    SetRawInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY,
                                     base::UTF8ToUTF16(country_code), status);
    return !GetRawInfo(ADDRESS_HOME_COUNTRY).empty();
  }

  SetRawInfoWithVerificationStatus(storable_type, value, status);

  // Give up when importing addresses with any entirely blank lines.
  // There's a good chance that this formatting is not intentional, but it's
  // also not obviously safe to just strip the newlines.
  if (storable_type == ADDRESS_HOME_STREET_ADDRESS) {
    return Root()->IsValueForTypeValid(ADDRESS_HOME_STREET_ADDRESS,
                                       /*wipe_if_not=*/true);
  }

  return true;
}

VerificationStatus Address::GetVerificationStatusImpl(FieldType type) const {
  return GetRoot().GetVerificationStatusForType(type);
}

void Address::SetAddressCountryCode(const std::u16string& country_code,
                                    VerificationStatus verification_status) {
  const AddressCountryCode new_address_country_code =
      AddressCountryCode(base::UTF16ToUTF8(country_code));

  // No restructuring is necessary if the new country is the same as the current
  // one. Only updating the verification status is required.
  if (GetAddressCountryCode() == new_address_country_code) {
    Root()->SetValueForType(ADDRESS_HOME_COUNTRY, country_code,
                            verification_status);
    return;
  }

  // No restructuring is necessary if both countries use the default hierarchy.
  if (IsLegacyAddress() &&
      !i18n_model_definition::IsCustomHierarchyAvailableForCountry(
          new_address_country_code)) {
    Root()->SetValueForType(ADDRESS_HOME_COUNTRY, country_code,
                            verification_status);
    return;
  }

  // Create an updated version of the internal hierarchy.
  AddressComponentsStore updated_address_component_store =
      i18n_model_definition::CreateAddressComponentModel(
          new_address_country_code);
  is_legacy_address_ =
      !i18n_model_definition::IsCustomHierarchyAvailableForCountry(
          new_address_country_code);

  // Transfer the content from the old model into the new one. Note that it
  // is possible that some nodes are not present in the updated model. Those
  // will be ignored.
  FieldTypeSet prev_supported_types;
  Root()->GetStorableTypes(&prev_supported_types);
  prev_supported_types.erase(ADDRESS_HOME_COUNTRY);

  for (FieldType type : prev_supported_types) {
    updated_address_component_store.Root()->SetValueForType(
        type, Root()->GetValueForType(type),
        Root()->GetVerificationStatusForType(type));
  }

  address_component_store_ = std::move(updated_address_component_store);
  // Update verification status.
  Root()->SetValueForType(ADDRESS_HOME_COUNTRY, country_code,
                          verification_status);
}

bool Address::IsAddressFieldSettingAccessible(FieldType field_type) const {
  // Default to US in case of empty country codes.
  AutofillCountry country(GetAddressCountryCode()->empty()
                              ? "US"
                              : GetAddressCountryCode().value());

  for (AddressComponent* component =
           address_component_store_.GetNodeForType(field_type);
       component != nullptr; component = component->Parent()) {
    if (country.IsAddressFieldSettingAccessible(component->GetStorageType())) {
      return true;
    }
  }
  return false;
}

}  // namespace autofill
