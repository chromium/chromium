// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_FOR_LOCALE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_COUNTRY_NAMES_FOR_LOCALE_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/span.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace autofill {

// This is a pseudo locale for the "Country name" -> "Country code" mapping.
// When using this locale, we map the name of each country in the languages
// spoken in the country to the country code. This is not exhaustive but
// based on registered locales. For example, for Italy, there may be ca-IT,
// de-IT and it-IT, but no fr-IT even though Italy has a French community.
// This is not perfect but a start.
constexpr const char kPseudoLocaleOfNativeTranslations[] = "native";

// TODO(crbug.com/1360502) Remove this after finishing the experiment. The
// purpose of this locale is just to provide a value for a default/control
// experiment group in which we don't have native translations of countries.
constexpr const char kPseudoLocaleOfNativeTranslationsDisabled[] =
    "native-disabled";

// Returns the locales installed on this computer.
base::span<const icu::Locale> GetAvailableLocales();

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
