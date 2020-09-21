// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESS_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/form_group.h"

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
  base::string16 GetRawInfo(ServerFieldType type) const override;
  void SetRawInfoWithVerificationStatus(
      ServerFieldType type,
      const base::string16& value,
      structured_address::VerificationStatus status) override;
  void GetMatchingTypes(const base::string16& text,
                        const std::string& locale,
                        ServerFieldTypeSet* matching_types) const override;

  void ResetStructuredTokes();

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

  // For structured addresses, returns true if |this| is mergeable with |newer|.
  bool IsStructuredAddressMergeable(const Address& newer) const;

  // Returns a constant reference to |structured_address_|.
  const structured_address::Address& GetStructuredAddress() const;

 private:
  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;
  base::string16 GetInfoImpl(const AutofillType& type,
                             const std::string& locale) const override;
  bool SetInfoWithVerificationStatusImpl(
      const AutofillType& type,
      const base::string16& value,
      const std::string& locale,
      structured_address::VerificationStatus status) override;

  // Return the verification status of a structured name value.
  structured_address::VerificationStatus GetVerificationStatusImpl(
      ServerFieldType type) const override;

  // Trims any trailing newlines from |street_address_|.
  void TrimStreetAddress();

  // TODO(crbug.com/1130194): Clean legacy implementation once structured
  // addresses are fully launched.
  // The lines of the street address.
  std::vector<base::string16> street_address_;
  // A subdivision of city, e.g. inner-city district or suburb.
  base::string16 dependent_locality_;
  base::string16 city_;
  base::string16 state_;
  base::string16 zip_code_;
  // Similar to a ZIP code, but used by entities that might not be
  // geographically contiguous.  The canonical example is CEDEX in France.
  base::string16 sorting_code_;

  // The following entries are only popluated by Sync and
  // used to create type votes, but are not used for filling fields.
  base::string16 street_name_;
  base::string16 dependent_street_name_;
  base::string16 house_number_;
  base::string16 premise_name_;
  base::string16 subpremise_;

  // The ISO 3166 2-letter country code, or an empty string if there is no
  // country data specified for this address.
  std::string country_code_;

  // This data structure holds the address information if the structured address
  // feature is enabled.
  structured_address::Address structured_address_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESS_H_
