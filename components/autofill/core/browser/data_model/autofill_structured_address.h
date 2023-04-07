// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"

namespace autofill {

// This class reimplements the ValueForComparison method to apply a
// country-specific rewriter to the normalized value.
class AddressComponentWithRewriter : public AddressComponent {
 public:
  using AddressComponent::AddressComponent;

 protected:
  // Apply a country-specific rewriter to the normalized value.
  std::u16string ValueForComparison(
      const AddressComponent& other) const override;

  // Applies the |country_code| specific rewriter to the normalized value. If
  // |country_code| is empty, it defaults to US.
  std::u16string RewriteValue(const std::u16string& value,
                              const std::u16string& country_code) const;
};

// The name of the street.
class StreetNameNode : public AddressComponent {
 public:
  explicit StreetNameNode(AddressComponent* parent);
  ~StreetNameNode() override;
};

// In some countries, addresses use the intersection of two streets.
// The DependentStreetName is represents the second street of the intersections.
class DependentStreetNameNode : public AddressComponent {
 public:
  explicit DependentStreetNameNode(AddressComponent* parent);
  ~DependentStreetNameNode() override;
};

// Contains both the StreetName and the DependentStreetName of an address.
class StreetAndDependentStreetNameNode : public AddressComponent {
 public:
  explicit StreetAndDependentStreetNameNode(AddressComponent* parent);
  ~StreetAndDependentStreetNameNode() override;

 private:
  StreetNameNode thoroughfare_name_{this};
  DependentStreetNameNode dependent_thoroughfare_name_{this};
};

// The house number. It also contains the subunit descriptor, e.g. the 'a' in
// '73a'.
class HouseNumberNode : public AddressComponent {
 public:
  explicit HouseNumberNode(AddressComponent* parent);
  ~HouseNumberNode() override;
};

// The name of the premise.
class PremiseNode : public AddressComponent {
 public:
  explicit PremiseNode(AddressComponent* parent);
  ~PremiseNode() override;
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

// The StreetAddress incorporates the StreetAndDependentStreetName, the
// HouseNumber, the PremiseName and SubPremise.
// This class inherits from AddressComponentWithRewriter to implement rewriting
// values for comparison.
class StreetAddressNode : public AddressComponentWithRewriter {
 public:
  explicit StreetAddressNode(AddressComponent* parent);
  ~StreetAddressNode() override;

  void GetAdditionalSupportedFieldTypes(
      ServerFieldTypeSet* supported_types) const override;

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

  // Returns the format string to create the full name from its subcomponents.
  std::u16string GetBestFormatString() const override;

  // Recalculates the address line after an assignment.
  void PostAssignSanitization() override;

  // Apply line-wise parsing of the street address as a fallback method.
  void ParseValueAndAssignSubcomponentsByFallbackMethod() override;

 protected:
  // Implements support for getting the value of the individual address lines.
  bool ConvertAndGetTheValueForAdditionalFieldTypeName(
      const std::string& type_name,
      std::u16string* value) const override;

  // Implements support for setting the value of the individual address lines.
  bool ConvertAndSetValueForAdditionalFieldTypeName(
      const std::string& type_name,
      const std::u16string& value,
      const VerificationStatus& status) override;

  // Returns true of the address lines do not contain an empty line.
  bool IsValueValid() const override;

 private:
  // Calculates the address line from the street address.
  void CalculateAddressLines();

  StreetAndDependentStreetNameNode streets_{this};
  HouseNumberNode number_{this};
  PremiseNode premise_{this};
  SubPremiseNode sub_premise_{this};

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
  std::u16string NormalizedValue() const override;
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

  void MigrateLegacyStructure(bool is_verified_profile) override;

  // Checks if the street address contains an invalid structure and wipes it if
  // necessary.
  bool WipeInvalidStructure() override;

 private:
  StreetAddressNode street_address_{this};
  PostalCodeNode postal_code_{this};
  SortingCodeNode sorting_code_{this};
  DependentLocalityNode dependent_locality_{this};
  CityNode city_{this};
  StateNode state_{this};
  CountryCodeNode country_code_{this};
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_
