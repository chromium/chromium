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
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/autofill/core/browser/geo/country_names_for_locale.h"

namespace autofill {

// A singleton class that encapsulates mappings from country names to their
// corresponding country codes.
class CountryNames {
 public:
  // The first call to this function, causing the creation of CountryNames,
  // is expensive.
  static CountryNames* GetInstance();

  CountryNames(const CountryNames&) = delete;
  CountryNames& operator=(const CountryNames&) = delete;

  // Returns the country code corresponding to the `country_name` queried for
  // the application and default locale.
  const std::string GetCountryCode(const std::u16string& country_name) const;

  // Returns the country code for a `country_name` provided with a
  // `locale_name`. If no country code can be determined, an empty string is
  // returned. The purpose of this method is to translate country names from a
  // locale different to one the instance was constructed for.
  const std::string GetCountryCodeForLocalizedCountryName(
      const std::u16string& country_name,
      const std::string& locale_name);

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

  friend base::NoDestructor<CountryNames>;

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
