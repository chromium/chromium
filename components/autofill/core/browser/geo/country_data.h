// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace autofill {

// The minimal required fields for an address to be complete for a given
// country.
enum AddressRequiredFields {
  ADDRESS_REQUIRES_CITY = 1 << 0,
  ADDRESS_REQUIRES_STATE = 1 << 1,
  ADDRESS_REQUIRES_ZIP = 1 << 2,

  // Composite versions (for data).
  ADDRESS_REQUIRES_CITY_STATE = ADDRESS_REQUIRES_CITY | ADDRESS_REQUIRES_STATE,
  ADDRESS_REQUIRES_STATE_ZIP = ADDRESS_REQUIRES_STATE | ADDRESS_REQUIRES_ZIP,
  ADDRESS_REQUIRES_CITY_ZIP = ADDRESS_REQUIRES_CITY | ADDRESS_REQUIRES_ZIP,
  ADDRESS_REQUIRES_CITY_STATE_ZIP =
      ADDRESS_REQUIRES_CITY | ADDRESS_REQUIRES_STATE | ADDRESS_REQUIRES_ZIP,

  // Policy for countries that don't have city, state or zip requirements.
  ADDRESS_REQUIRES_ADDRESS_LINE_1_ONLY = 0,

  // Policy for countries for which we do not have information about valid
  // address format.
  ADDRESS_REQUIREMENTS_UNKNOWN = ADDRESS_REQUIRES_CITY_STATE_ZIP,
};

// This struct describes the address format typical for a particular country.
struct CountryData {
  // Resource identifier for the string used to denote postal codes.
  int postal_code_label_id;

  // Resource identifier for the string used to denote the major subdivision
  // below the "country" level.
  int state_label_id;

  // The required parts of the address.
  AddressRequiredFields address_required_fields;
};

// A singleton class that encapsulates a map from country codes to country data.
class CountryDataMap {
 public:
  static CountryDataMap* GetInstance();

  const std::map<std::string, CountryData>& country_data() {
    return country_data_;
  }

  const std::vector<std::string>& country_codes() { return country_codes_; }

 private:
  CountryDataMap();
  ~CountryDataMap();
  friend struct base::DefaultSingletonTraits<CountryDataMap>;

  const std::map<std::string, CountryData> country_data_;
  const std::vector<std::string> country_codes_;

  DISALLOW_COPY_AND_ASSIGN(CountryDataMap);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_DATA_H_
