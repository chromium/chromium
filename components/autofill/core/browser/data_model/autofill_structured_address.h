// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// This class reimplements the ValueForComparison method to apply a
// country-specific rewriter to the normalized value.
class AddressComponentWithRewriter : public AddressComponent {
 public:
  using AddressComponent::AddressComponent;

 protected:
  // Normalizes and then applies a country-specific rewriter to the `value`
  // provided.
  std::u16string GetValueForComparison(
      const std::u16string& value,
      const AddressComponent& other) const override;
};

// This class represents a type that is controlled by a feature flag. It
// overrides the SetValue method to prevent setting values to nodes for which
// the flag is turned off.
class FeatureGuardedAddressComponent : public AddressComponent {
 public:
  FeatureGuardedAddressComponent(raw_ptr<const base::Feature> feature,
                                 ServerFieldType storage_type,
                                 AddressComponent* parent,
                                 unsigned int merge_mode);

  // Sets the value corresponding to the storage type of this component.
  void SetValue(std::u16string value, VerificationStatus status) override;

 private:
  // Feature guarding the rollout of this address component.
  const raw_ptr<const base::Feature> feature_;
};

// The name of the street.
class StreetNameNode : public AddressComponent {
 public:
  explicit StreetNameNode(AddressComponent* parent);
  ~StreetNameNode() override;
};

// The house number. It also contains the subunit descriptor, e.g. the 'a' in
// '73a'.
class HouseNumberNode : public AddressComponent {
 public:
  explicit HouseNumberNode(AddressComponent* parent);
  ~HouseNumberNode() override;
};

// Contains both the StreetName and the HouseNumberNode of an address.
class StreetLocationNode : public AddressComponent {
 public:
  explicit StreetLocationNode(AddressComponent* parent);
  ~StreetLocationNode() override;

 private:
  StreetNameNode street_name_{this};
  HouseNumberNode house_number_{this};
};

// The floor the apartment is located in.
class FloorNode : public AddressComponent {
 public:
  explicit FloorNode(AddressComponent* parent);
  ~FloorNode() override;
};

// The number of the apartment.
class ApartmentNode : public AddressComponent {
 public:
  explicit ApartmentNode(AddressComponent* parent);
  ~ApartmentNode() override;
};

// The SubPremise contains the floor and the apartment number.
class SubPremiseNode : public AddressComponent {
 public:
  explicit SubPremiseNode(AddressComponent* parent);
  ~SubPremiseNode() override;

 private:
  FloorNode floor_{this};
  ApartmentNode apartment_{this};
};

// Stores the landmark of an address profile.
class LandmarkNode : public FeatureGuardedAddressComponent {
 public:
  explicit LandmarkNode(AddressComponent* parent);
  ~LandmarkNode() override;
};

// Stores the streets intersection of an address profile.
class BetweenStreetsNode : public FeatureGuardedAddressComponent {
 public:
  explicit BetweenStreetsNode(AddressComponent* parent);
  ~BetweenStreetsNode() override;
};

// Stores administrative area level 2. A sub-division of a state, e.g. a
// Municipio in Brazil or Mexico.
class AdminLevel2Node : public FeatureGuardedAddressComponent {
 public:
  explicit AdminLevel2Node(AddressComponent* parent);
  ~AdminLevel2Node() override;
};

// The StreetAddress incorporates the StreetLocation, BetweenStreets, Landmark
// and SubPremise.
// This class inherits from AddressComponentWithRewriter to implement rewriting
// values for comparison.
class StreetAddressNode : public AddressComponentWithRewriter {
 public:
  explicit StreetAddressNode(AddressComponent* parent);
  ~StreetAddressNode() override;

  const ServerFieldTypeSet GetAdditionalSupportedFieldTypes() const override;

  void SetValue(std::u16string value, VerificationStatus status) override;

  void UnsetValue() override;

 protected:
  // Gives the component with the higher verification status precedence.
  // If the statuses are the same, the older component gets precedence if it
  // contains newlines but the newer one does not.
  bool HasNewerValuePrecedenceInMerging(
      const AddressComponent& newer_component) const override;

  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

  // Recalculates the address line after an assignment.
  void PostAssignSanitization() override;

  // Apply line-wise parsing of the street address as a fallback method.
  void ParseValueAndAssignSubcomponentsByFallbackMethod() override;

 protected:
  // Implements support for getting the value of the individual address lines.
  std::u16string GetValueForOtherSupportedType(
      ServerFieldType field_type) const override;

  // Implements support for setting the value of the individual address lines.
  void SetValueForOtherSupportedType(ServerFieldType field_type,
                                     const std::u16string& value,
                                     const VerificationStatus& status) override;

  // Returns true of the address lines do not contain an empty line.
  bool IsValueValid() const override;

 private:
  // Calculates the address line from the street address.
  void CalculateAddressLines();

  // Returns the corresponding address line depending on `type`. Assumes that
  // `type` is ADDRESS_HOME_LINE(1|2|3).
  std::u16string GetAddressLine(ServerFieldType type) const;

  StreetLocationNode street_location_{this};
  BetweenStreetsNode between_streets_{this};
  SubPremiseNode sub_premise_{this};
  LandmarkNode landmark_code_{this};

  // Holds the values of the individual address lines.
  // Must be recalculated if the value of the component changes.
  std::vector<std::u16string> address_lines_;
};

// Stores the country code of an address profile.
class CountryCodeNode : public AddressComponent {
 public:
  explicit CountryCodeNode(AddressComponent* parent);
  ~CountryCodeNode() override;
};

// Stores the city of an address.
class DependentLocalityNode : public AddressComponent {
 public:
  explicit DependentLocalityNode(AddressComponent* parent);
  ~DependentLocalityNode() override;
};

// Stores the city of an address.
class CityNode : public AddressComponent {
 public:
  explicit CityNode(AddressComponent* parent);
  ~CityNode() override;
};

// Stores the state of an address.
// This class inherits from AddressComponentWithRewriter to implement rewriting
// values for comparison.
class StateNode : public AddressComponentWithRewriter {
 public:
  explicit StateNode(AddressComponent* parent);
  ~StateNode() override;

  // For states we use the AlternativeStateNameMap to offer canonicalized state
  // names.
  absl::optional<std::u16string> GetCanonicalizedValue() const override;
};

// Stores the postal code of an address.
// This class inherits from AddressComponentWithRewriter to implement rewriting
// values for comparison.
class PostalCodeNode : public AddressComponentWithRewriter {
 public:
  explicit PostalCodeNode(AddressComponent* parent);
  ~PostalCodeNode() override;

 protected:
  // In contrast to the base class, the normalization removes all white spaces
  // from the value.
  std::u16string GetNormalizedValue() const override;

  std::u16string GetValueForComparison(
      const std::u16string& value,
      const AddressComponent& other) const override;
};

// Stores the sorting code.
class SortingCodeNode : public AddressComponent {
 public:
  explicit SortingCodeNode(AddressComponent* parent);
  ~SortingCodeNode() override;
};

// Stores the overall Address that contains the StreetAddress, the PostalCode
// the City, the State and the CountryCode.
class AddressNode : public AddressComponent {
 public:
  AddressNode();
  AddressNode(const AddressNode& other);
  explicit AddressNode(AddressComponent* parent);
  AddressNode& operator=(const AddressNode& other);
  ~AddressNode() override;

  void MigrateLegacyStructure() override;

  // Checks if the street address contains an invalid structure and wipes it if
  // necessary.
  bool WipeInvalidStructure() override;

 private:
  StreetAddressNode street_address_{this};
  CityNode city_{this};
  DependentLocalityNode dependent_locality_{this};
  StateNode state_{this};
  AdminLevel2Node admin_level_2_{this};
  PostalCodeNode postal_code_{this};
  SortingCodeNode sorting_code_{this};
  CountryCodeNode country_code_{this};
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_
