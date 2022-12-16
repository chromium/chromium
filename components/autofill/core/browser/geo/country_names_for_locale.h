// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_FOR_LOCALE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_FOR_LOCALE_H_

#include <map>
#include <memory>
#include <string>

#include "third_party/icu/source/i18n/unicode/coll.h"

namespace autofill {

class CountryNamesForLocale {
 public:
  // Create |CountryNamesForLocale| for the supplied |locale_name|.
  explicit CountryNamesForLocale(const std::string& locale_name);

  CountryNamesForLocale(CountryNamesForLocale&& source);

  ~CountryNamesForLocale();

  // Returns the country code corresponding to the |country_name| localized to
  // |locale_name_|. Returns an empty string if no matching country code can be
  // found.
  const std::string GetCountryCode(const std::u16string& country_name) const;

 private:
  // Returns an ICU collator -- i.e. string comparator -- appropriate for the
  // given |locale|, or null if no collator is available.
  const icu::Collator* GetCollatorForLocale(const icu::Locale& locale);

  // The name of the locale the instance was constructed for.
  std::string locale_name_;

  // Collator for the locale.
  std::unique_ptr<icu::Collator> collator_;

  // Maps from localized country names in the supplied locale to their
  // corresponding country codes. The keys are ICU collation sort keys
  // corresponding to the target localized country name.
  std::map<std::string, std::string> localized_names_;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_FOR_LOCALE_H_
