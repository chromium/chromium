// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/country_info.h"

#include <string>

#include "base/strings/string_util.h"
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

CountryInfo::CountryInfo(const CountryInfo& info) = default;

CountryInfo& CountryInfo::operator=(const CountryInfo& info) = default;

CountryInfo::CountryInfo(CountryInfo&& info) = default;

CountryInfo& CountryInfo::operator=(CountryInfo&& info) = default;

CountryInfo::~CountryInfo() = default;

std::u16string CountryInfo::GetCountryName(
    const std::string& app_locale) const {
  return AutofillCountry(country_code_, app_locale).name();
}

std::string CountryInfo::GetCountryCode() const {
  return country_code_;
}

bool CountryInfo::SetCountryFromCountryName(const std::u16string& value,
                                            const std::string& app_locale) {
  if (CountryNames* country_names =
          !value.empty() ? CountryNames::GetInstance() : nullptr) {
    country_code_ =
        country_names->GetCountryCodeForLocalizedCountryName(value, app_locale);
    return !country_code_.empty();
  }
  return false;
}

bool CountryInfo::SetCountryFromCountryCode(const std::u16string& value) {
  std::u16string value_upper = base::ToUpperASCII(value);
  if (data_util::IsValidCountryCode(value_upper)) {
    country_code_ = base::UTF16ToUTF8(value_upper);
    return true;
  }
  return false;
}

}  // namespace autofill
