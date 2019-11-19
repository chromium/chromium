// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_names.h"

#include <stdint.h>

#include <memory>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// A copy of the application locale string, which should be ready for
// CountryName's construction.
static base::LazyInstance<std::string>::DestructorAtExit g_application_locale =
    LAZY_INSTANCE_INITIALIZER;

// Returns the ICU sort key corresponding to |str| for the given |collator|.
// Uses |buffer| as temporary storage, and might resize |buffer| as a side-
// effect. |buffer_size| should specify the |buffer|'s size, and is updated if
// the |buffer| is resized.
const std::string GetSortKey(const icu::Collator& collator,
                             const base::string16& str,
                             std::unique_ptr<uint8_t[]>* buffer,
                             int32_t* buffer_size) {
  DCHECK(buffer);
  DCHECK(buffer_size);

  icu::UnicodeString icu_str(str.c_str(), str.length());
  int32_t expected_size =
      collator.getSortKey(icu_str, buffer->get(), *buffer_size);
  if (expected_size > *buffer_size) {
    // If there wasn't enough space, grow the buffer and try again.
    *buffer_size = expected_size;
    *buffer = std::make_unique<uint8_t[]>(*buffer_size);
    DCHECK(buffer->get());

    expected_size = collator.getSortKey(icu_str, buffer->get(), *buffer_size);
    DCHECK_EQ(*buffer_size, expected_size);
  }

  return std::string(reinterpret_cast<const char*>(buffer->get()));
}

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
#if defined(OS_IOS)
  // iOS uses the Foundation API to get the localized display name, in which
  // "China" is named "Chine mainland".
  common_names.insert(std::make_pair("CHINA", "CN"));
#endif
  return common_names;
}

// Creates collator for |locale| and sets its attributes as needed.
std::unique_ptr<icu::Collator> CreateCollator(const icu::Locale& locale) {
  std::unique_ptr<icu::Collator> collator(
      autofill::l10n::GetCollatorForLocale(locale));
  if (!collator)
    return nullptr;

  // Compare case-insensitively and ignoring punctuation.
  UErrorCode ignored = U_ZERO_ERROR;
  collator->setAttribute(UCOL_STRENGTH, UCOL_SECONDARY, ignored);
  ignored = U_ZERO_ERROR;
  collator->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, ignored);

  return collator;
}

// If |locale| is different from "en_US", returns a collator for "en_US" and
// sets its attributes as appropriate. Otherwise returns null.
std::unique_ptr<icu::Collator> CreateDefaultCollator(
    const icu::Locale& locale) {
  icu::Locale default_locale("en_US");

  if (default_locale != locale)
    return CreateCollator(default_locale);

  return nullptr;
}

// Returns the mapping of country names localized to |locale| to their
// corresponding country codes. The provided |collator| should be suitable for
// the locale. The collator being null is handled gracefully by returning an
// empty map, to account for the very rare cases when the collator fails to
// initialize.
std::map<std::string, std::string> GetLocalizedNames(
    const std::string& locale,
    const icu::Collator* collator) {
  if (!collator)
    return std::map<std::string, std::string>();

  std::map<std::string, std::string> localized_names;
  int32_t buffer_size = 1000;
  auto buffer = std::make_unique<uint8_t[]>(buffer_size);

  for (const std::string& country_code :
       CountryDataMap::GetInstance()->country_codes()) {
    base::string16 country_name =
        l10n_util::GetDisplayNameForCountry(country_code, locale);
    std::string sort_key =
        GetSortKey(*collator, country_name, &buffer, &buffer_size);

    localized_names.insert(std::make_pair(sort_key, country_code));
  }
  return localized_names;
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
    : locale_(locale_name.c_str()),
      collator_(CreateCollator(locale_)),
      default_collator_(CreateDefaultCollator(locale_)),
      common_names_(GetCommonNames()),
      localized_names_(GetLocalizedNames(locale_name, collator_.get())),
      default_localized_names_(
          GetLocalizedNames("en_US", default_collator_.get())) {}

CountryNames::CountryNames() : CountryNames(g_application_locale.Get()) {
  DCHECK(!g_application_locale.Get().empty());
}

CountryNames::~CountryNames() = default;

const std::string CountryNames::GetCountryCode(const base::string16& country) {
  // First, check common country names, including 2- and 3-letter country codes.
  std::string country_utf8 = base::UTF16ToUTF8(base::ToUpperASCII(country));
  const auto result = common_names_.find(country_utf8);
  if (result != common_names_.end())
    return result->second;

  // Next, check country names localized to the current locale.
  std::string country_code =
      GetCountryCodeForLocalizedName(country, localized_names_, *collator_);
  if (!country_code.empty())
    return country_code;

  // Finally, check country names localized to US English, unless done already.
  if (default_collator_) {
    return GetCountryCodeForLocalizedName(country, default_localized_names_,
                                          *default_collator_);
  }

  return std::string();
}

const std::string CountryNames::GetCountryCodeForLocalizedName(
    const base::string16& country_name,
    const std::map<std::string, std::string>& localized_names,
    const icu::Collator& collator) {
  // As recommended[1] by ICU, initialize the buffer size to four times the
  // source string length.
  // [1] http://userguide.icu-project.org/collation/api#TOC-Examples
  int32_t buffer_size = country_name.size() * 4;
  auto buffer = std::make_unique<uint8_t[]>(buffer_size);
  std::string sort_key =
      GetSortKey(collator, country_name, &buffer, &buffer_size);

  auto result = localized_names.find(sort_key);

  if (result != localized_names.end())
    return result->second;

  return std::string();
}

}  // namespace autofill
