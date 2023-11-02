// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"

namespace autofill {
namespace structured_address {

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

  void SetValue(std::u16string value, VerificationStatus status) override;

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

  StreetAndDependentStreetName streets_{this};
  HouseNumber number_{this};
  Premise premise_{this};
  SubPremise sub_premise_{this};

  // Holds the values of the individual address lines.
  // Must be recalculated if the value of the component changes.
  std::vector<std::u16string> address_lines_;
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

  // For states we use the AlternativeStateNameMap to offer canonicalized state
  // names.
  absl::optional<std::u16string> GetCanonicalizedValue() const override;
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
  std::u16string NormalizedValue() const override;
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
  Address& operator=(const Address& other);

  void MigrateLegacyStructure(bool is_verified_profile) override;

  // Checks if the street address contains an invalid structure and wipes it if
  // necessary.
  bool WipeInvalidStructure() override;

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
