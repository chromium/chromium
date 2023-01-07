// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESS_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"

namespace autofill {

// A form group that stores address information.
class Address : public FormGroup {
 public:
  Address();
  Address(const Address& address);
  ~Address() override;

  Address& operator=(const Address& address);
  bool operator==(const Address& other) const;
  bool operator!=(const Address& other) const { return !operator==(other); }

  // FormGroup:
  std::u16string GetRawInfo(ServerFieldType type) const override;
  void SetRawInfoWithVerificationStatus(
      ServerFieldType type,
      const std::u16string& value,
      structured_address::VerificationStatus status) override;
  void GetMatchingTypes(const std::u16string& text,
                        const std::string& locale,
                        ServerFieldTypeSet* matching_types) const override;

  // Derives all missing tokens in the structured representation of the address
  // either parsing missing tokens from their assigned parent or by formatting
  // them from their assigned children.
  bool FinalizeAfterImport(bool profile_is_verified);

  // Convenience wrapper to invoke finalization for unverified profiles.
  bool FinalizeAfterImport() { return FinalizeAfterImport(false); }

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
  const structured_address::Address& GetStructuredAddress() const;

 private:
  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;
  std::u16string GetInfoImpl(const AutofillType& type,
                             const std::string& locale) const override;
  bool SetInfoWithVerificationStatusImpl(
      const AutofillType& type,
      const std::u16string& value,
      const std::string& locale,
      structured_address::VerificationStatus status) override;

  // Return the verification status of a structured name value.
  structured_address::VerificationStatus GetVerificationStatusImpl(
      ServerFieldType type) const override;

  // This data structure holds the address information if the structured address
  // feature is enabled.
  structured_address::Address structured_address_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESS_H_
