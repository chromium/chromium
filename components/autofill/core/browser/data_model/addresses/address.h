// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_ADDRESS_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component_store.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"

namespace autofill {

// A form group that stores address information.
class Address : public FormGroup {
 public:
  // See `AutofillProfile::kDatabaseStoredTypes` for a documentation of the
  // purpose of this constant.
  static constexpr FieldTypeSet kDatabaseStoredTypes{
      ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_STREET_NAME,
      ADDRESS_HOME_STREET_LOCATION,
      ADDRESS_HOME_HOUSE_NUMBER,
      ADDRESS_HOME_HOUSE_NUMBER_AND_APT,
      ADDRESS_HOME_SUBPREMISE,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      ADDRESS_HOME_SORTING_CODE,
      ADDRESS_HOME_COUNTRY,
      ADDRESS_HOME_APT,
      ADDRESS_HOME_APT_NUM,
      ADDRESS_HOME_APT_TYPE,
      ADDRESS_HOME_FLOOR,
      ADDRESS_HOME_OVERFLOW,
      ADDRESS_HOME_LANDMARK,
      ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
      ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
      ADDRESS_HOME_BETWEEN_STREETS,
      ADDRESS_HOME_BETWEEN_STREETS_1,
      ADDRESS_HOME_BETWEEN_STREETS_2,
      ADDRESS_HOME_ADMIN_LEVEL2,
      ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY};

  explicit Address(AddressCountryCode country_code);
  ~Address() override;

  Address(const Address& address);
  Address& operator=(const Address& address);
  bool operator==(const Address& other) const;

  // FormGroup:
  std::u16string GetInfo(const AutofillType& type,
                         const std::string& app_locale) const override;
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  // TODO(crbug.com/40264633): Change `AutofillType` into `FieldType`.
  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     const std::u16string& value,
                                     const std::string& locale,
                                     VerificationStatus status) override;
  void GetMatchingTypes(const std::u16string& text,
                        const std::string& locale,
                        FieldTypeSet* matching_types) const override;
  // Return the verification status of a structured name value.
  VerificationStatus GetVerificationStatus(FieldType type) const override;

  // Derives all missing tokens in the structured representation of the address
  // either parsing missing tokens from their assigned parent or by formatting
  // them from their assigned children.
  bool FinalizeAfterImport();

  // For structured addresses, merges |newer| into |this|. For some values
  // within the structured address tree the more recently used profile gets
  // precedence. |newer_was_more_recently_used| indicates if the newer was also
  // more recently used.
  bool MergeStructuredAddress(const Address& newer,
                              bool newer_was_more_recently_used);

  // Fetches the canonical state name for the current address object if
  // possible.
  std::optional<AlternativeStateNameMap::CanonicalStateName>
  GetCanonicalizedStateName() const;

  // For structured addresses, returns true if |this| is mergeable with |newer|.
  bool IsStructuredAddressMergeable(const Address& newer) const;
  // Like `IsStructuredAddressMergeable()`, but only for the subtree
  // corresponding to `type`.
  bool IsStructuredAddressMergeableForType(FieldType type,
                                           const Address& other) const;

  // Returns a constant reference to the structured address' root node (i.e.
  // ADDRESS_HOME_ADDRESS) from the nodes store.
  const AddressComponent& GetRoot() const;

  // Returns the structured address country code.
  AddressCountryCode GetAddressCountryCode() const;

  // Returns whether the structured address uses the legacy address hierarchy.
  bool IsLegacyAddress() const { return is_legacy_address_; }

  // Returns true if the given `field_type` is part of Autofill's address
  // model for `GetAddressCountryCode()` and is accessible via settings. Note
  // that a field can also be settings accessible via a different field that is
  // at a higher level in the address hierarchy tree. The function returns true
  // in this case as well.
  bool IsAddressFieldSettingAccessible(FieldType field_type) const;

 private:
  // FormGroup:
  FieldTypeSet GetSupportedTypes() const override;

  // Updates the address' country, builds the hierarchy model corresponding to
  // `country_code` and transfers the content of the old data model into the new
  // one.
  void SetAddressCountryCode(const std::u16string& country_code,
                             VerificationStatus status);

  // Returns a pointer to the structured address' root node (i.e.
  // ADDRESS_HOME_ADDRESS) from the nodes store.
  AddressComponent* Root();

  // This data structure holds the structured address information.
  AddressComponentsStore address_component_store_;

  // Whether the structured address uses the legacy hierarchy.
  bool is_legacy_address_ = true;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_ADDRESS_H_
