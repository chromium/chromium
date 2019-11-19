// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace autofill {

// A singleton class that encapsulates mappings from country names to their
// corresponding country codes.
class CountryNames {
 public:
  // The first call to this function, causing the creation of CountryNames,
  // is expensive.
  static CountryNames* GetInstance();

  // Tells CountryNames, what is the application locale. Only the first supplied
  // value is used, further calls result in no changes.  Call this on the UI
  // thread, before first using CountryNames. |locale| must not be empty.
  static void SetLocaleString(const std::string& locale);

  // Returns the country code corresponding to |country|, which should be a
  // country code or country name localized to |locale_name|.
  const std::string GetCountryCode(const base::string16& country);

 protected:
  // Create CountryNames for |locale_name|. Protected for testing.
  explicit CountryNames(const std::string& locale_name);

  // Protected for testing.
  ~CountryNames();

 private:
  // Create CountryNames for the default locale.
  CountryNames();

  friend struct base::DefaultSingletonTraits<CountryNames>;

  // Looks up |country_name| in |localized_names|, using |collator| and
  // returns the corresponding country code or an empty string if there is
  // none.
  const std::string GetCountryCodeForLocalizedName(
      const base::string16& country_name,
      const std::map<std::string, std::string>& localized_names,
      const icu::Collator& collator);

  // Returns an ICU collator -- i.e. string comparator -- appropriate for the
  // given |locale|, or null if no collator is available.
  const icu::Collator* GetCollatorForLocale(const icu::Locale& locale);

  // The locale object for the application locale string.
  const icu::Locale locale_;

  // Collator for the application locale.
  const std::unique_ptr<icu::Collator> collator_;

  // Collator for the "en_US" locale, if different from the application
  // locale, null otherwise.
  const std::unique_ptr<icu::Collator> default_collator_;

  // Maps from common country names, including 2- and 3-letter country codes,
  // to the corresponding 2-letter country codes. The keys are uppercase ASCII
  // strings.
  const std::map<std::string, std::string> common_names_;

  // Maps from localized country names (in the application locale) to their
  // corresponding country codes. The keys are ICU collation sort keys
  // corresponding to the target localized country name.
  const std::map<std::string, std::string> localized_names_;

  // The same as |localized_names_| but for the "en_US" locale. Empty if
  // "en_US" is the application locale already.
  const std::map<std::string, std::string> default_localized_names_;

  DISALLOW_COPY_AND_ASSIGN(CountryNames);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_H_
