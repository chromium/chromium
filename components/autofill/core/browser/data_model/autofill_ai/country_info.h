// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_COUNTRY_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_COUNTRY_INFO_H_

#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// A form group that stores country information. This class is used to represent
// country information that is not part of any larger address structure. This is
// not related to `CountryCodeNode` which is the country information of a full
// address that contains other information like street address, city, zip code,
// etc. The class exists as a restrictive version of `Address` that only allows
// reading/writing country information.
class CountryInfo : public FormGroup {
 public:
  // Types whose info should be stored in the database for a `CountryInfo`
  // object, to ensure correct reconstruction while reading from the database.
  static constexpr FieldTypeSet kDatabaseStoredTypes{ADDRESS_HOME_COUNTRY};

  CountryInfo();
  CountryInfo(const CountryInfo& info);
  CountryInfo& operator=(const CountryInfo& info);
  CountryInfo(CountryInfo&& info);
  CountryInfo& operator=(CountryInfo&& info);
  ~CountryInfo() override;

  // FormGroup:
  // Returns the country name.
  std::u16string GetInfo(const AutofillType& type,
                         const std::string& app_locale) const override;
  // Returns the country code.
  std::u16string GetRawInfo(FieldType type) const override;
  // Sets the country code given a country code.
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;
  // Sets the country code given a country code or name.
  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     const std::u16string& value,
                                     const std::string& app_locale,
                                     VerificationStatus status) override;
  // Trivially returns `VerificationStatus::kNoStatus` since this class doesn't
  // require verification statuses.
  VerificationStatus GetVerificationStatus(FieldType type) const override;

  friend bool operator==(const CountryInfo&, const CountryInfo&);

 private:
  // FormGroup:
  FieldTypeSet GetSupportedTypes() const override;

  // The two-letter ISO-3166 country code.
  std::string country_code_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_COUNTRY_INFO_H_
