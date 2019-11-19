// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_AUTOFILL_COUNTRY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_AUTOFILL_COUNTRY_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/geo/country_data.h"

namespace autofill {

// Stores data associated with a country. Strings are localized to the app
// locale.
class AutofillCountry {
 public:
  // Returns country data corresponding to the two-letter ISO code
  // |country_code|.
  AutofillCountry(const std::string& country_code, const std::string& locale);
  ~AutofillCountry();

  // Returns the likely country code for |locale|, or "US" as a fallback if no
  // mapping from the locale is available.
  static const std::string CountryCodeForLocale(const std::string& locale);

  const std::string& country_code() const { return country_code_; }
  const base::string16& name() const { return name_; }
  const base::string16& postal_code_label() const { return postal_code_label_; }
  const base::string16& state_label() const { return state_label_; }

  // City is expected in a complete address for this country.
  bool requires_city() const {
    return (address_required_fields_ & ADDRESS_REQUIRES_CITY) != 0;
  }

  // State is expected in a complete address for this country.
  bool requires_state() const {
    return (address_required_fields_ & ADDRESS_REQUIRES_STATE) != 0;
  }

  // Zip is expected in a complete address for this country.
  bool requires_zip() const {
    return (address_required_fields_ & ADDRESS_REQUIRES_ZIP) != 0;
  }

 private:
  AutofillCountry(const std::string& country_code,
                  const base::string16& name,
                  const base::string16& postal_code_label,
                  const base::string16& state_label);

  // The two-letter ISO-3166 country code.
  std::string country_code_;

  // The country's name, localized to the app locale.
  base::string16 name_;

  // The localized label for the postal code (or zip code) field.
  base::string16 postal_code_label_;

  // The localized label for the state (or province, district, etc.) field.
  base::string16 state_label_;

  // Address requirement field codes for the country.
  AddressRequiredFields address_required_fields_;

  DISALLOW_COPY_AND_ASSIGN(AutofillCountry);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_AUTOFILL_COUNTRY_H_
