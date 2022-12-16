// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_names_for_locale.h"

#include <map>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {
// Returns the ICU sort key corresponding to |str| for the given |collator|.
// Uses |buffer| as temporary storage, and might resize |buffer| as a side-
// effect. |buffer_size| should specify the |buffer|'s size, and is updated if
// the |buffer| is resized.
const std::string GetSortKey(const icu::Collator& collator,
                             const std::u16string& str,
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

// Creates collator for |locale| and sets its attributes as needed.
std::unique_ptr<icu::Collator> CreateCollator(const icu::Locale& locale) {
  std::unique_ptr<icu::Collator> collator(l10n::GetCollatorForLocale(locale));
  if (!collator)
    return nullptr;

  // Compare case-insensitively and ignoring punctuation.
  UErrorCode ignored = U_ZERO_ERROR;
  collator->setAttribute(UCOL_STRENGTH, UCOL_SECONDARY, ignored);
  ignored = U_ZERO_ERROR;
  collator->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, ignored);

  return collator;
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
    std::u16string country_name =
        l10n_util::GetDisplayNameForCountry(country_code, locale);
    std::string sort_key =
        GetSortKey(*collator, country_name, &buffer, &buffer_size);

    localized_names.insert(std::make_pair(sort_key, country_code));
  }
  return localized_names;
}

}  // namespace

CountryNamesForLocale::CountryNamesForLocale(const std::string& locale_name)
    : locale_name_(locale_name),
      collator_(CreateCollator(locale_name_.c_str())),
      localized_names_(GetLocalizedNames(locale_name, collator_.get())) {}

CountryNamesForLocale::~CountryNamesForLocale() = default;

CountryNamesForLocale::CountryNamesForLocale(CountryNamesForLocale&& source)
    : locale_name_(std::move(source.locale_name_)),
      collator_(std::move(source.collator_)),
      localized_names_(std::move(source.localized_names_)) {}

const std::string CountryNamesForLocale::GetCountryCode(
    const std::u16string& country_name) const {
  // As recommended[1] by ICU, initialize the buffer size to four times the
  // source string length.
  // [1] http://userguide.icu-project.org/collation/api#TOC-Examples
  if (!collator_)
    return std::string();

  int32_t buffer_size = country_name.size() * 4;
  auto buffer = std::make_unique<uint8_t[]>(buffer_size);
  std::string sort_key =
      GetSortKey(*collator_, country_name, &buffer, &buffer_size);

  auto result = localized_names_.find(sort_key);

  if (result != localized_names_.end())
    return result->second;

  return std::string();
}

}  // namespace autofill
