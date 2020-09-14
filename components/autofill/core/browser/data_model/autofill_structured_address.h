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
class StreetAddress : public AddressComponent {
 public:
  explicit StreetAddress(AddressComponent* parent);
  ~StreetAddress() override;

 protected:
  std::vector<const re2::RE2*> GetParseRegularExpressionsByRelevance()
      const override;

  // Returns the format string to create the full name from its subcomponents.
  base::string16 GetBestFormatString() const override;

 private:
  StreetAndDependentStreetName streets_{this};
  HouseNumber number_{this};
  Premise premise_{this};
  SubPremise sub_premise_{this};
};

// Stores the country code of an address profile.
class CountryCode : public AddressComponent {
 public:
  explicit CountryCode(AddressComponent* parent);
  ~CountryCode() override;
};

// Stores the city of an address.
class City : public AddressComponent {
 public:
  explicit City(AddressComponent* parent);
  ~City() override;
};

// Stores the state of an address.
class State : public AddressComponent {
 public:
  explicit State(AddressComponent* parent);
  ~State() override;
};

// Stores the postal code of an address.
class PostalCode : public AddressComponent {
 public:
  explicit PostalCode(AddressComponent* parent);
  ~PostalCode() override;
};

// Stores the overall Address that contains the StreetAddress, the PostalCode
// the City, the State and the CountryCode.
class Address : public AddressComponent {
 public:
  Address();
  explicit Address(AddressComponent* parent);
  ~Address() override;

 private:
  StreetAddress street_address_{this};
  PostalCode postal_code_{this};
  City city_{this};
  State state_{this};
  CountryCode country_code_{this};
};

}  // namespace structured_address

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_H_
