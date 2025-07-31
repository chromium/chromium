// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_names.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/i18n/case_conversion.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/geo/country_names_for_locale.h"
#include "components/autofill/core/browser/geo/country_native_names_inl.h"

namespace autofill {

namespace {

constexpr std::string_view kDefaultLocale = "en-US";

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
      base::i18n::GetConfiguredLocale());
  return instance.get();
}

CountryNames::CountryNames(std::string locale_name)
    : application_locale_name_(std::move(locale_name)),
      country_names_for_default_locale_(std::string(kDefaultLocale)),
      country_names_for_application_locale_(application_locale_name_),
      common_names_(GetCommonNames()),
      localized_country_names_cache_(10) {}

CountryNames::~CountryNames() = default;

const std::string& CountryNames::GetCountryCode(
    std::u16string_view country) const {
  // First, check common country names, including 2- and 3-letter country codes.
  std::string country_utf8 = base::UTF16ToUTF8(base::ToUpperASCII(country));
  const auto result = common_names_.find(country_utf8);
  if (result != common_names_.end())
    return result->second;

  // Next, check country native names.
  const auto native_result =
      kCountryNativeNames.find(base::i18n::ToUpper(country));
  if (native_result != kCountryNativeNames.end()) {
    return native_result->second;
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
  // Terminate if a country code was determined or if the locale matches the
  // default ones.
  if (!localized_result.empty() || locale_name == application_locale_name_ ||
      locale_name == kDefaultLocale || locale_name.empty()) {
    return localized_result;
  }
  // Acquire a lock for the localization cache.
  base::AutoLock lock(localized_country_names_cache_lock_);

  // Lookup the CountryName for the locale in the cache.
  auto iter = localized_country_names_cache_.Get(locale_name);
  if (iter != localized_country_names_cache_.end())
    return iter->second.GetCountryCode(country);

  // Put the country names for the locale into the cache.
  auto it = localized_country_names_cache_.Put(
      locale_name, CountryNamesForLocale(std::string(locale_name)));
  const CountryNamesForLocale& country_names_for_locale = it->second;
  return country_names_for_locale.GetCountryCode(country);
}

}  // namespace autofill
