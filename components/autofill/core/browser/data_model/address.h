// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESS_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"

namespace autofill {

// A form group that stores address information.
class Address : public FormGroup {
 public:
  explicit Address(AddressCountryCode country_code);
  ~Address() override;

  Address(const Address& address);
  Address& operator=(const Address& address);
  bool operator==(const Address& other) const;
  bool operator!=(const Address& other) const { return !operator==(other); }

  // FormGroup:
  std::u16string GetRawInfo(ServerFieldType type) const override;
  void SetRawInfoWithVerificationStatus(ServerFieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  void GetMatchingTypes(const std::u16string& text,
                        const std::string& locale,
                        ServerFieldTypeSet* matching_types) const override;

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
  absl::optional<AlternativeStateNameMap::CanonicalStateName>
  GetCanonicalizedStateName() const;

  // For structured addresses, returns true if |this| is mergeable with |newer|.
  bool IsStructuredAddressMergeable(const Address& newer) const;

  // Returns a constant reference to |structured_address_|.
  const AddressComponent& GetStructuredAddress() const;

  // Returns the structured address country code.
  AddressCountryCode GetAddressCountryCode() const;

  // Returns whether the structured address uses the legacy address hierarchy.
  bool IsLegacyAddress() const { return is_legacy_address_; }

 private:
  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;
  std::u16string GetInfoImpl(const AutofillType& type,
                             const std::string& locale) const override;
  bool SetInfoWithVerificationStatusImpl(const AutofillType& type,
                                         const std::u16string& value,
                                         const std::string& locale,
                                         VerificationStatus status) override;

  // Return the verification status of a structured name value.
  VerificationStatus GetVerificationStatusImpl(
      ServerFieldType type) const override;

  // Updates the address' country, builds the hierarchy model corresponding to
  // `country_code` and transfers the content of the old data model into the new
  // one.
  void SetAddressCountryCode(const std::u16string& country_code,
                             VerificationStatus status);

  // This data structure holds the address information if the structured address
  // feature is enabled.
  std::unique_ptr<AddressComponent> structured_address_;

  // Whether the structured address uses the legacy hierarchy.
  bool is_legacy_address_ = true;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESS_H_
