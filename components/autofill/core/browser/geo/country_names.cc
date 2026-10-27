// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_names.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/map_util.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "base/i18n/tag_converters.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/geo/country_names_for_locale.h"
#include "components/autofill/core/browser/geo/country_native_names_inl.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace autofill {

namespace {

constexpr base::i18n::LanguageTag kDefaultLocale =
    base::i18n::GetKnownLanguageTag("en-US");

// Computes the value for CountryNames::common_names_.
std::map<std::string, std::string> GetCommonNames() {
  std::map<std::string, std::string> common_names;
  // Add 2- and 3-letter ISO country codes.
  for (const std::string& country_code :
       CountryDataMap::GetInstance()->country_codes()) {
    common_names.insert(std::make_pair(country_code, country_code));

    std::string iso3_country_code =
        icu::Locale(nullptr, country_code.c_str()).getISO3Country();

    // ICU list of countries can be out-of-date with CLDR.
    if (!iso3_country_code.empty())
      common_names.insert(std::make_pair(iso3_country_code, country_code));
  }

  // Add a few other common synonyms.
  common_names.insert(std::make_pair("UNITED STATES OF AMERICA", "US"));
  common_names.insert(std::make_pair("U.S.A.", "US"));
  common_names.insert(std::make_pair("GREAT BRITAIN", "GB"));
  common_names.insert(std::make_pair("UK", "GB"));
  // For some reason this is not provided by ICU:
  common_names.insert(std::make_pair("CZECH REPUBLIC", "CZ"));
#if BUILDFLAG(IS_IOS)
  // iOS uses the Foundation API to get the localized display name, in which
  // "China" is named "Chine mainland".
  common_names.insert(std::make_pair("CHINA", "CN"));
#endif
  return common_names;
}

}  // namespace

// static
CountryNames* CountryNames::GetInstance() {
  static base::NoDestructor<CountryNames> instance(
      base::i18n::GetDefaultIcuLocale());
  return instance.get();
}

CountryNames::CountryNames(const base::i18n::LanguageTag& locale)
    : application_locale_(locale),
      country_names_for_default_locale_(kDefaultLocale),
      country_names_for_application_locale_(locale),
      common_names_(GetCommonNames()),
      localized_country_names_cache_(10) {}

CountryNames::~CountryNames() = default;

const std::string& CountryNames::GetCountryCode(
    std::u16string_view country) const {
  // First, check common country names, including 2- and 3-letter country codes.
  std::string country_utf8 = base::UTF16ToUTF8(base::ToUpperASCII(country));
  if (const std::string* result =
          base::FindOrNull(common_names_, country_utf8)) {
    return *result;
  }

  // Next, check country native names.
  if (const std::string* native_result =
          base::FindOrNull(kCountryNativeNames, base::i18n::ToUpper(country))) {
    return *native_result;
  }

  // Next, check country names localized to the current locale.
  const std::string& country_code =
      country_names_for_application_locale_.GetCountryCode(country);
  if (!country_code.empty())
    return country_code;

  // Finally, check country names localized to US English, unless done already.
  return country_names_for_default_locale_.GetCountryCode(country);
}

const std::string& CountryNames::GetCountryCodeForLocalizedCountryName(
    std::u16string_view country,
    std::string_view locale_name) {
  // Do an unconditional lookup using the default and app_locale.
  // Chances are that the name of the country matches the localized one.
  const std::string& localized_result = GetCountryCode(country);
  if (!localized_result.empty()) {
    return localized_result;
  }
  // Terminate if the locale is invalid or matches the default ones.
  const std::optional<base::i18n::LanguageTag> locale =
      base::i18n::GetLanguageTagFromString(locale_name);
  if (!locale || *locale == application_locale_ || *locale == kDefaultLocale) {
    return localized_result;
  }
  // Acquire a lock for the localization cache.
  base::AutoLock lock(localized_country_names_cache_lock_);

  // Lookup the CountryName for the locale in the cache.
  auto iter = localized_country_names_cache_.Get(locale_name);
  if (iter != localized_country_names_cache_.end())
    return iter->second.GetCountryCode(country);

  // Put the country names for the locale into the cache.
  auto it = localized_country_names_cache_.Put(locale_name,
                                               CountryNamesForLocale(*locale));
  const CountryNamesForLocale& country_names_for_locale = it->second;
  return country_names_for_locale.GetCountryCode(country);
}

}  // namespace autofill
