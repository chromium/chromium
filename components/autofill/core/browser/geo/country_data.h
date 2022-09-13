// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_DATA_H_

#include <map>
#include <string>
#include <vector>

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace autofill {

// The minimal required fields for an address to be complete for a given
// country.
enum RequiredFieldsForAddressImport {
  ADDRESS_REQUIRES_CITY = 1 << 0,
  ADDRESS_REQUIRES_STATE = 1 << 1,
  ADDRESS_REQUIRES_ZIP = 1 << 2,
  ADDRESS_REQUIRES_LINE1 = 1 << 3,
  ADDRESS_REQUIRES_ZIP_OR_STATE = 1 << 4,
  ADDRESS_REQUIRES_LINE1_OR_HOUSE_NUMBER = 1 << 5,

  // Composite versions (for data).
  ADDRESS_REQUIRES_LINE1_CITY =
      (ADDRESS_REQUIRES_LINE1 | ADDRESS_REQUIRES_CITY),
  ADDRESS_REQUIRES_LINE1_ZIP = (ADDRESS_REQUIRES_LINE1 | ADDRESS_REQUIRES_ZIP),
  ADDRESS_REQUIRES_LINE1_STATE =
      (ADDRESS_REQUIRES_LINE1 | ADDRESS_REQUIRES_STATE),

  ADDRESS_REQUIRES_LINE1_CITY_STATE =
      (ADDRESS_REQUIRES_LINE1 | ADDRESS_REQUIRES_CITY | ADDRESS_REQUIRES_STATE),
  ADDRESS_REQUIRES_LINE1_STATE_ZIP =
      (ADDRESS_REQUIRES_LINE1 | ADDRESS_REQUIRES_STATE | ADDRESS_REQUIRES_ZIP),
  ADDRESS_REQUIRES_LINE1_CITY_ZIP =
      (ADDRESS_REQUIRES_LINE1 | ADDRESS_REQUIRES_CITY | ADDRESS_REQUIRES_ZIP),
  ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP =
      (ADDRESS_REQUIRES_LINE1 | ADDRESS_REQUIRES_CITY | ADDRESS_REQUIRES_STATE |
       ADDRESS_REQUIRES_ZIP),

  ADDRESS_REQUIRES_LINE1_CITY_AND_ZIP_OR_STATE =
      ADDRESS_REQUIRES_LINE1_CITY | ADDRESS_REQUIRES_ZIP_OR_STATE,

  ADDRESS_REQUIRES_ZIP_AND_LINE1_OR_HOUSE_NUMBER =
      ADDRESS_REQUIRES_ZIP | ADDRESS_REQUIRES_LINE1_OR_HOUSE_NUMBER,

  // Policy for countries for which we do not have information about valid
  // address format.
  ADDRESS_REQUIREMENTS_UNKNOWN = ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP,
};

// A singleton class that encapsulates a map from country codes to country data.
class CountryDataMap {
 public:
  static CountryDataMap* GetInstance();

  CountryDataMap(const CountryDataMap&) = delete;
  CountryDataMap& operator=(const CountryDataMap&) = delete;

  // Returns true if a |CountryData| entry for the supplied |country_code|
  // exists.
  bool HasRequiredFieldsForAddressImport(const std::string& country_code) const;

  // Returns true if there is a country code alias for |country_code|.
  bool HasCountryCodeAlias(const std::string& country_code_alias) const;

  // Returns the country code for a country code alias. If no alias definition
  // is present return an empty string.
  const std::string GetCountryCodeForAlias(
      const std::string& country_code_alias) const;

  // Lookup the |RequiredFieldForAddressImport| for the supplied |country_code|.
  // If no entry exists, return requirements for the US as a best guess.
  RequiredFieldsForAddressImport GetRequiredFieldsForAddressImport(
      const std::string& country_code) const;

  // Return a constant reference to a vector of all country codes.
  const std::vector<std::string>& country_codes() { return country_codes_; }

 private:
  CountryDataMap();
  ~CountryDataMap();
  friend struct base::DefaultSingletonTraits<CountryDataMap>;

  const std::map<std::string, RequiredFieldsForAddressImport>
      required_fields_for_address_import_map_;
  const std::map<std::string, std::string> country_code_aliases_;
  const std::vector<std::string> country_codes_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_DATA_H_
