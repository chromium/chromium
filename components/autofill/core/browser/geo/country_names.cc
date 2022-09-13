// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_names.h"

#include <map>
#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {
namespace {

// A copy of the application locale string, which should be ready for
// CountryName's construction.
static base::LazyInstance<std::string>::DestructorAtExit g_application_locale =
    LAZY_INSTANCE_INITIALIZER;

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
  common_names.insert(std::make_pair("BRASIL", "BR"));
  common_names.insert(std::make_pair("DEUTSCHLAND", "DE"));
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
  return base::Singleton<CountryNames>::get();
}

// static
void CountryNames::SetLocaleString(const std::string& locale) {
  DCHECK(!locale.empty());
  // Application locale should never be empty. The empty value of
  // |g_application_locale| means that it has not been initialized yet.
  std::string* storage = g_application_locale.Pointer();
  if (storage->empty()) {
    *storage = locale;
  }
  // TODO(crbug.com/579971) CountryNames currently cannot adapt to changed
  // locale without Chrome's restart.
}

CountryNames::CountryNames(const std::string& locale_name)
    : application_locale_name_(locale_name),
      default_locale_name_(std::string("en_US")),
      country_names_for_default_locale_(default_locale_name_),
      country_names_in_local_languages_(
          base::FeatureList::IsEnabled(features::kAutofillCountryFromLocalName)
              ? kPseudoLocaleOfNativeTranslations
              : kPseudoLocaleOfNativeTranslationsDisabled),
      country_names_for_application_locale_(application_locale_name_),
      common_names_(GetCommonNames()),
      localized_country_names_cache_(10) {}

CountryNames::CountryNames() : CountryNames(g_application_locale.Get()) {
  DCHECK(!g_application_locale.Get().empty());
}

CountryNames::~CountryNames() = default;

const std::string CountryNames::GetCountryCode(
    const std::u16string& country,
    DetectionOfCountryName* source) const {
  // First, check common country names, including 2- and 3-letter country codes.
  std::string country_utf8 = base::UTF16ToUTF8(base::ToUpperASCII(country));
  const auto result = common_names_.find(country_utf8);
  if (result != common_names_.end()) {
    if (source)
      *source = DetectionOfCountryName::kCommonNames;
    return result->second;
  }

  // Next, check country names localized to the current locale.
  std::string country_code =
      country_names_for_application_locale_.GetCountryCode(country);
  if (!country_code.empty()) {
    if (source)
      *source = DetectionOfCountryName::kApplicationLocale;
    return country_code;
  }

  if (base::FeatureList::IsEnabled(features::kAutofillCountryFromLocalName)) {
    // If that did not find anything try the names of countries (Austria speaks
    // German and uses "Österreich" for their own country).
    country_code = country_names_in_local_languages_.GetCountryCode(country);
    if (!country_code.empty()) {
      if (source)
        *source = DetectionOfCountryName::kLocalLanguages;
      return country_code;
    }
  }

  // Finally, check country names localized to US English, unless done already.
  country_code = country_names_for_default_locale_.GetCountryCode(country);
  if (!country_code.empty()) {
    if (source)
      *source = DetectionOfCountryName::kDefaultLocale;
  }
  return country_code;
}

const std::string CountryNames::GetCountryCodeForLocalizedCountryName(
    const std::u16string& country,
    const std::string& locale_name,
    DetectionOfCountryName* source) {
  // Do an unconditional lookup using the default and app_locale.
  // Chances are that the name of the country matches the localized one.
  std::string result = GetCountryCode(country, source);
  // Terminate if a country code was determined or if the locale matches the
  // default ones.
  if (!result.empty() || locale_name == application_locale_name_ ||
      locale_name == default_locale_name_ || locale_name.empty()) {
    return result;
  }
  // Acquire a lock for the localization cache.
  base::AutoLock lock(localized_country_names_cache_lock_);

  // Lookup the CountryName for the locale in the cache.
  auto iter = localized_country_names_cache_.Get(locale_name);
  if (iter != localized_country_names_cache_.end()) {
    if (source)
      *source = DetectionOfCountryName::kViaLanguageOfWebsite;
    return iter->second.GetCountryCode(country);
  }

  CountryNamesForLocale country_names_for_locale(locale_name);
  result = country_names_for_locale.GetCountryCode(country);

  // Put the country names for the locale into the cache.
  localized_country_names_cache_.Put(locale_name,
                                     std::move(country_names_for_locale));

  if (!result.empty()) {
    if (source)
      *source = DetectionOfCountryName::kViaLanguageOfWebsite;
  } else {
    if (source)
      *source = DetectionOfCountryName::kNotFound;
  }

  return result;
}

}  // namespace autofill
