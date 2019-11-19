// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/autofill_country.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// The maximum capacity needed to store a locale up to the country code.
const size_t kLocaleCapacity =
    ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY + 1;

}  // namespace

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const std::string& locale) {
  auto result =
      CountryDataMap::GetInstance()->country_data().find(country_code);
  DCHECK(result != CountryDataMap::GetInstance()->country_data().end());
  const CountryData& data = result->second;

  country_code_ = country_code;
  name_ = l10n_util::GetDisplayNameForCountry(country_code, locale);
  postal_code_label_ = l10n_util::GetStringUTF16(data.postal_code_label_id);
  state_label_ = l10n_util::GetStringUTF16(data.state_label_id);
  address_required_fields_ = data.address_required_fields;
}

AutofillCountry::~AutofillCountry() {}

// static
const std::string AutofillCountry::CountryCodeForLocale(
    const std::string& locale) {
  // Add likely subtags to the locale. In particular, add any likely country
  // subtags -- e.g. for locales like "ru" that only include the language.
  std::string likely_locale;
  UErrorCode error_ignored = U_ZERO_ERROR;
  uloc_addLikelySubtags(locale.c_str(),
                        base::WriteInto(&likely_locale, kLocaleCapacity),
                        kLocaleCapacity, &error_ignored);

  // Extract the country code.
  std::string country_code = icu::Locale(likely_locale.c_str()).getCountry();

  // Default to the United States if we have no better guess.
  if (!base::Contains(CountryDataMap::GetInstance()->country_codes(),
                      country_code)) {
    return "US";
  }

  return country_code;
}

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const base::string16& name,
                                 const base::string16& postal_code_label,
                                 const base::string16& state_label)
    : country_code_(country_code),
      name_(name),
      postal_code_label_(postal_code_label),
      state_label_(state_label) {}

}  // namespace autofill
