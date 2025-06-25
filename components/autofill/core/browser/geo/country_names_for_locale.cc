// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_names_for_locale.h"

#include <map>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {
// Returns the ICU sort key corresponding to `str` for the given `collator`.
// Uses `buffer` as temporary storage, and might resize `buffer` as a side-
// effect.
std::string GetSortKey(const icu::Collator& collator,
                       std::u16string_view str,
                       base::HeapArray<uint8_t>& buffer) {
  icu::UnicodeString icu_str(str.data(), str.length());
  size_t expected_size = static_cast<size_t>(
      collator.getSortKey(icu_str, buffer.data(), buffer.size()));
  if (expected_size > buffer.size()) {
    // If there wasn't enough space, grow the buffer and try again.
    buffer = base::HeapArray<uint8_t>::WithSize(expected_size);
    DCHECK(buffer.data());

    expected_size = collator.getSortKey(icu_str, buffer.data(), buffer.size());
    DCHECK_EQ(buffer.size(), expected_size);
  }
  return std::string(base::as_string_view(buffer.first(expected_size)));
}

// Creates collator for `locale` and sets its attributes as needed.
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

// Returns the mapping of country names localized to `locale` to their
// corresponding country codes. The provided `collator` should be suitable for
// the locale. The collator being null is handled gracefully by returning an
// empty map, to account for the very rare cases when the collator fails to
// initialize.
std::map<std::string, std::string> GetLocalizedNames(
    std::string_view locale,
    const icu::Collator* collator) {
  if (!collator)
    return std::map<std::string, std::string>();

  std::map<std::string, std::string> localized_names;
  auto buffer = base::HeapArray<uint8_t>::WithSize(1000);

  for (const std::string& country_code :
       CountryDataMap::GetInstance()->country_codes()) {
    std::u16string country_name =
        l10n_util::GetDisplayNameForCountry(country_code, locale);
    localized_names.insert(std::make_pair(
        GetSortKey(*collator, country_name, buffer), country_code));
  }
  return localized_names;
}

}  // namespace

CountryNamesForLocale::CountryNamesForLocale(std::string locale_name)
    : locale_name_(std::move(locale_name)),
      collator_(CreateCollator(locale_name_.c_str())),
      localized_names_(GetLocalizedNames(locale_name_, collator_.get())) {}

CountryNamesForLocale::CountryNamesForLocale(CountryNamesForLocale&&) = default;

CountryNamesForLocale::~CountryNamesForLocale() = default;

const std::string& CountryNamesForLocale::GetCountryCode(
    std::u16string_view country_name) const {
  // As recommended[1] by ICU, initialize the buffer size to four times the
  // source string length.
  // [1] http://userguide.icu-project.org/collation/api#TOC-Examples
  if (!collator_) {
    return base::EmptyString();
  }

  auto buffer = base::HeapArray<uint8_t>::WithSize(country_name.size() * 4);
  const std::string sort_key = GetSortKey(*collator_, country_name, buffer);
  if (auto it = localized_names_.find(sort_key); it != localized_names_.end()) {
    return it->second;
  }

  return base::EmptyString();
}

}  // namespace autofill
