// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_COUNTRY_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_COUNTRY_INFO_H_

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// A class that stores country information. This class is used to represent
// country information that is not part of any larger address structure. This is
// not related to `CountryCodeNode` which is the country information of a full
// address that contains other information like street address, city, zip code,
// etc. The class exists as a restrictive version of `Address` that only allows
// reading/writing country information.
class CountryInfo {
 public:
  // Types whose info should be stored in the database for a `CountryInfo`
  // object, to ensure correct reconstruction while reading from the database.
  static constexpr FieldTypeSet kDatabaseStoredTypes{ADDRESS_HOME_COUNTRY};

  CountryInfo();
  CountryInfo(const CountryInfo& info);
  CountryInfo& operator=(const CountryInfo& info);
  CountryInfo(CountryInfo&& info);
  CountryInfo& operator=(CountryInfo&& info);
  ~CountryInfo();

  // Returns the stored country, represented as a name localized to
  // `app_locale`.
  std::u16string GetCountryName(const std::string& app_locale) const;
  // Returns the stored country code, in the two-letter ISO-3166 format.
  std::string GetCountryCode() const;

  // Returns true if `country_code_` is set according to `country_name` and
  // `app_locale`, i.e. the write ioperation s successful, and false otherwise.
  bool SetCountryFromCountryName(const std::u16string& country_name,
                                 const std::string& app_locale);
  // Returns true if `country_code_` is set according to `country_code`, i.e.
  // the write operation is successful, and false otherwise.
  bool SetCountryFromCountryCode(const std::u16string& country_code);

  friend bool operator==(const CountryInfo&, const CountryInfo&) = default;

 private:
  // The two-letter ISO-3166 country code.
  std::string country_code_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_COUNTRY_INFO_H_
