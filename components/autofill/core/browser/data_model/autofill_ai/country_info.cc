// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/country_info.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"

namespace autofill {

CountryInfo::CountryInfo() = default;

CountryInfo::CountryInfo(const CountryInfo& info) = default;

CountryInfo& CountryInfo::operator=(const CountryInfo& info) = default;

CountryInfo::CountryInfo(CountryInfo&& info) = default;

CountryInfo& CountryInfo::operator=(CountryInfo&& info) = default;

CountryInfo::~CountryInfo() = default;

std::u16string CountryInfo::GetCountryName(std::string_view app_locale) const {
  return AutofillCountry(country_code_, app_locale).name();
}

std::string CountryInfo::GetCountryCode() const {
  return country_code_;
}

bool CountryInfo::SetCountryFromCountryName(std::u16string_view value,
                                            std::string_view app_locale) {
  if (value.empty()) {
    return false;
  }
  if (CountryNames* country_names = CountryNames::GetInstance()) {
    country_code_ =
        country_names->GetCountryCodeForLocalizedCountryName(value, app_locale);
    return !country_code_.empty();
  }
  return false;
}

bool CountryInfo::SetCountryFromCountryCode(std::u16string_view value) {
  std::u16string value_upper = base::ToUpperASCII(value);
  if (!data_util::IsValidCountryCode(value_upper)) {
    return false;
  }
  country_code_ = base::UTF16ToUTF8(value_upper);
  return true;
}

}  // namespace autofill
