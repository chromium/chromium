// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/country_info.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"

namespace autofill {

CountryInfo::CountryInfo() = default;

CountryInfo::CountryInfo(const CountryInfo& info)
    : country_code_(info.country_code_) {}

CountryInfo::~CountryInfo() = default;

bool operator==(const CountryInfo& lhs, const CountryInfo& rhs) {
  return lhs.country_code_ == rhs.country_code_;
}

std::u16string CountryInfo::GetRawInfo(FieldType type) const {
  CHECK_EQ(ADDRESS_HOME_COUNTRY, type);
  return base::UTF8ToUTF16(country_code_);
}

void CountryInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                   const std::u16string& value,
                                                   VerificationStatus status) {
  CHECK_EQ(ADDRESS_HOME_COUNTRY, type);
  country_code_ = data_util::IsValidCountryCode(value)
                      ? base::UTF16ToUTF8(value)
                      : std::string();
}

FieldTypeSet CountryInfo::GetSupportedTypes() const {
  static constexpr FieldTypeSet supported_types{ADDRESS_HOME_COUNTRY};
  return supported_types;
}

std::u16string CountryInfo::GetInfo(const AutofillType& type,
                                    const std::string& app_locale) const {
  CHECK_EQ(ADDRESS_HOME_COUNTRY, type.GetStorableType());
  return AutofillCountry(country_code_, app_locale).name();
}

bool CountryInfo::SetInfoWithVerificationStatus(const AutofillType& type,
                                                const std::u16string& value,
                                                const std::string& app_locale,
                                                VerificationStatus status) {
  CHECK_EQ(ADDRESS_HOME_COUNTRY, type.GetStorableType());
  if (data_util::IsValidCountryCode(value)) {
    country_code_ = base::UTF16ToUTF8(value);
    return true;
  }

  if (CountryNames* country_names =
          !value.empty() ? CountryNames::GetInstance() : nullptr) {
    country_code_ =
        country_names->GetCountryCodeForLocalizedCountryName(value, app_locale);
  }

  return !country_code_.empty();
}

VerificationStatus CountryInfo::GetVerificationStatus(FieldType type) const {
  return VerificationStatus::kNoStatus;
}

}  // namespace autofill
