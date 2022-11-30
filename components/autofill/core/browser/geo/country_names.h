// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/lru_cache.h"
#include "base/synchronization/lock.h"
#include "components/autofill/core/browser/geo/country_names_for_locale.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace autofill {

// An enum for histogram to record which source for country names resolved
// a string.
// TODO(crbug.com/1360502) Delete when the feature landed.
enum class DetectionOfCountryName {
  // A country was resolved via the hardcoded common names.
  kCommonNames = 0,
  // A country was resolved by a lookup of the name using the application
  // locale.
  kApplicationLocale = 1,
  // A country was resolved by a lookup of the name using a map, where each
  // country is represented in the main languages spoken by in the country.
  kLocalLanguages = 2,
  // A country was resolved by lookup using the en_US locale.
  kDefaultLocale = 3,
  // A country was resolved by a lookup using the language of the website.
  kViaLanguageOfWebsite = 4,
  // A country name candidate could not be resolved to a country code.
  kNotFound = 5,
  kMaxValue = kNotFound,
};

// A singleton class that encapsulates mappings from country names to their
// corresponding country codes.
class CountryNames {
 public:
  // The first call to this function, causing the creation of CountryNames,
  // is expensive.
  static CountryNames* GetInstance();

  CountryNames(const CountryNames&) = delete;
  CountryNames& operator=(const CountryNames&) = delete;

  // Tells CountryNames, what is the application locale. Only the first supplied
  // value is used, further calls result in no changes.  Call this on the UI
  // thread, before first using CountryNames. `locale` must not be empty.
  static void SetLocaleString(const std::string& locale);

  // Returns the country code corresponding to the `country_name` queried for
  // the application and default locale.
  // TODO(crbug.com/1360502): Remove `source`. If it is not null and the country
  // name was resolved, the first source that could resolve the name is
  // stored into `source`.
  const std::string GetCountryCode(
      const std::u16string& country_name,
      DetectionOfCountryName* source = nullptr) const;

  // Returns the country code for a `country_name` provided with a
  // `locale_name`. If no country code can be determined, an empty string is
  // returned. The purpose of this method is to translate country names from a
  // locale different to one the instance was constructed for.
  // TODO(crbug.com/1360502): Remove `source`. If it is not null and the country
  // name was resolved, the first source that could resolve the name is
  // stored into `source`.
  const std::string GetCountryCodeForLocalizedCountryName(
      const std::u16string& country_name,
      const std::string& locale_name,
      DetectionOfCountryName* source = nullptr);

#if defined(UNIT_TEST)
  // Returns true if the country names for the locale_name are in the cache.
  // Only used for testing.
  bool IsCountryNamesForLocaleCachedForTesting(const std::string& locale_name) {
    auto iter = localized_country_names_cache_.Get(locale_name);
    return iter != localized_country_names_cache_.end();
  }
#endif

 protected:
  // Create CountryNames for `locale_name`. Protected for testing.
  explicit CountryNames(const std::string& locale_name);

  // Protected for testing.
  ~CountryNames();

 private:
  // Create CountryNames for the default locale.
  CountryNames();

  friend struct base::DefaultSingletonTraits<CountryNames>;

  // Caches localized country name for a locale that is neither the application
  // or default locale. The Cache is keyed by the locale_name and contains
  // `CountryNamesForLocale` instances.
  using LocalizedCountryNamesCache =
      base::LRUCache<std::string, CountryNamesForLocale>;

  // The locale object for the application locale string.
  const std::string application_locale_name_;

  // The locale object for the default locale string.
  const std::string default_locale_name_;

  // Maps country names localized for the default locale to country codes.
  const CountryNamesForLocale country_names_for_default_locale_;

  // Maps country names localized in the languages of the respective countries
  // to country codes.
  // For example: The locale de_AT represents "German in Austria", the German
  // word for Austria is "Österreich", so country_names_in_local_languages_ will
  // map "Österreich" to "AT".
  // This is useful if a user visits an Austrian website where 1) Chrome runs
  // in en_EN locale and 2) Chrome did not recognize the website's language
  // (which defaults to "und" for undetermined).
  // In this case,
  // 1) default_locale_name_ (which is en_US) does not recognize the term,
  // 2) country_names_for_application_locale_ also relies on en_US, and finally
  // 3) localized_country_names_cache_ uses "und" as a language code and will
  //    also fail.
  // This map considers all languages spoken in a country. So on a Mac, we have
  // the following locales for Italy: ca-IT, de-IT, it-IT and as a
  // result, ["Itàlia", "Italien", "Italia"]. All mapped to "IT".
  const CountryNamesForLocale country_names_in_local_languages_;

  // Maps country names localized for the application locale to country codes.
  const CountryNamesForLocale country_names_for_application_locale_;

  // Maps from common country names, including 2- and 3-letter country codes,
  // to the corresponding 2-letter country codes. The keys are uppercase ASCII
  // strings.
  const std::map<std::string, std::string> common_names_;

  // A MRU cache to store localized strings for non-default locale lookups.
  LocalizedCountryNamesCache localized_country_names_cache_;

  // A lock for accessing and manipulating the localization cache.
  base::Lock localized_country_names_cache_lock_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_H_
