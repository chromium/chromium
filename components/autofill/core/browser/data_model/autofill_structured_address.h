// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"

using autofill::structured_address::AddressComponent;

namespace autofill {
namespace structured_address {

// This class reimplements the ValueForComparison method to apply a
// country-specific rewriter to the normalized value.
class AddressComponentWithRewriter : public AddressComponent {
 public:
  using AddressComponent::AddressComponent;

 protected:
  // Apply a country-specific rewriter to the normalized value.
  base::string16 ValueForComparison() const override;

  // Tries to retrieve the |ADDRESS_HOME_COUNTRY| node from the structure tree
  // to apply a country-specific rewriter to the normalized value.
  // If the country value cannot be retrieved or is empty, the method returns
  // the normalized values without further processing.
  base::string16 RewriteValue(const base::string16&) const;
};

// The name of the street.
class StreetName : public AddressComponent {
 public:
  explicit StreetName(AddressComponent* parent);
  ~StreetName() override;
};

// In some countries, addresses use the intersection of two streets.
// The DependentStreetName is represents the second street of the intersections.
class DependentStreetName : public AddressComponent {
 public:
  explicit DependentStreetName(AddressComponent* parent);
  ~DependentStreetName() override;
};

// Contains both the StreetName and the DependentStreetName of an address.
class StreetAndDependentStreetName : public AddressComponent {
 public:
  explicit StreetAndDependentStreetName(AddressComponent* parent);
  ~StreetAndDependentStreetName() override;

 private:
  StreetName thoroughfare_name_{this};
  DependentStreetName dependent_thoroughfare_name_{this};
};

// The house number. It also contains the subunit descriptor, e.g. the 'a' in
// '73a'.
class HouseNumber : public AddressComponent {
 public:
  explicit HouseNumber(AddressComponent* parent);
  ~HouseNumber() override;
};

// The name of the premise.
class Premise : public AddressComponent {
 public:
  explicit Premise(AddressComponent* parent);
  ~Premise() override;
};

// The floor the apartment is located in.
class Floor : public AddressComponent {
 public:
  explicit Floor(AddressComponent* parent);
  ~Floor() override;
};

// The number of the apartment.
class Apartment : public AddressComponent {
 public:
  explicit Apartment(AddressComponent* parent);
  ~Apartment() override;
};

// The SubPremise contains the floor and the apartment number.
class SubPremise : public AddressComponent {
 public:
  explicit SubPremise(AddressComponent* parent);
  ~SubPremise() override;

 private:
  Floor floor_{this};
  Apartment apartment_{this};
};

// The StreetAddress incorporates the StreetAndDependentStreetName, the
// HouseNumber, the PremiseName and SubPremise.
// This class inherits from AddressComponentWithRewriter to implement rewriting
// values for comparison.
class StreetAddress : public AddressComponentWithRewriter {
 public:
  explicit StreetAddress(AddressComponent* parent);
  ~StreetAddress() override;

  void GetAdditionalSupportedFieldTypes(
      ServerFieldTypeSet* supported_types) const override;

  void SetValue(base::string16 value, VerificationStatus status) override;

  void UnsetValue() override;

 protected:
  // Gives the component with the higher verification status precedence.
  // If the statuses are the same, the older component gets precedence if it
  // contains newlines but the newer one does not.
  bool HasNewerValuePrecendenceInMerging(
      const AddressComponent& newer_component) const override;

  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

  // Returns the format string to create the full name from its subcomponents.
  base::string16 GetBestFormatString() const override;

  // Recalculates the address line after an assignment.
  void PostAssignSanitization() override;

  // Apply line-wise parsing of the street address as a fallback method.
  void ParseValueAndAssignSubcomponentsByFallbackMethod() override;

 protected:
  // Implements support for getting the value of the individual address lines.
  bool ConvertAndGetTheValueForAdditionalFieldTypeName(
      const std::string& type_name,
      base::string16* value) const override;

  // Implements support for setting the value of the individual address lines.
  bool ConvertAndSetValueForAdditionalFieldTypeName(
      const std::string& type_name,
      const base::string16& value,
      const VerificationStatus& status) override;

  // Returns true of the address lines do not contain an empty line.
  bool IsValueValid() const override;

 private:
  // Calculates the address line from the street address.
  void CalculateAddressLines();

  StreetAndDependentStreetName streets_{this};
  HouseNumber number_{this};
  Premise premise_{this};
  SubPremise sub_premise_{this};

  // Holds the values of the individual address lines.
  // Must be recalculated if the value of the component changes.
  std::vector<base::string16> address_lines_;
};

// Stores the country code of an address profile.
class CountryCode : public AddressComponent {
 public:
  explicit CountryCode(AddressComponent* parent);
  ~CountryCode() override;
};

// Stores the city of an address.
class DependentLocality : public AddressComponent {
 public:
  explicit DependentLocality(AddressComponent* parent);
  ~DependentLocality() override;
};

// Stores the city of an address.
class City : public AddressComponent {
 public:
  explicit City(AddressComponent* parent);
  ~City() override;
};

// Stores the state of an address.
// This class inherits from AddressComponentWithRewriter to implement rewriting
// values for comparison.
class State : public AddressComponentWithRewriter {
 public:
  explicit State(AddressComponent* parent);
  ~State() override;
};

// Stores the postal code of an address.
// This class inherits from AddressComponentWithRewriter to implement rewriting
// values for comparison.
class PostalCode : public AddressComponentWithRewriter {
 public:
  explicit PostalCode(AddressComponent* parent);
  ~PostalCode() override;

 protected:
  // In contrast to the base class, the normalization removes all white spaces
  // from the value.
  base::string16 NormalizedValue() const override;
};

// Stores the sorting code.
class SortingCode : public AddressComponent {
 public:
  explicit SortingCode(AddressComponent* parent);
  ~SortingCode() override;
};

// Stores the overall Address that contains the StreetAddress, the PostalCode
// the City, the State and the CountryCode.
class Address : public AddressComponent {
 public:
  Address();
  Address(const Address& other);
  explicit Address(AddressComponent* parent);
  ~Address() override;

  // Migrates from a legacy structure in which name tokens are imported without
  // a status.
  void MigrateLegacyStructure(bool is_verified_profile);

 private:
  StreetAddress street_address_{this};
  PostalCode postal_code_{this};
  SortingCode sorting_code_{this};
  DependentLocality dependent_locality_{this};
  City city_{this};
  State state_{this};
  CountryCode country_code_{this};
};

}  // namespace structured_address

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_
