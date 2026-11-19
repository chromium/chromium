// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/transliterator.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/transliterator.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

// The transliteration rule to be applied.
enum class TransliterationId {
  // ICU Katakana-Hiragana transliteration.
  kKatakanaToHiragana,
  // ICU Hiragana-Katakana transliteration.
  kHiraganaToKatakana,
  // Simplified version of the ICU "::de-ASCII" transliteration.
  kGerman,
  // Converts to lowercase and removes diacritics.
  kDefault,
};

// List of country codes where the `TransliterationId::kGerman` can be applied.
static constexpr auto kCountriesWithGermanTransliteration =
    base::MakeFixedFlatSet<std::string_view>(
        {"AT", "BE", "CH", "DE", "IT", "LI", "LU"});

base::flat_map<TransliterationId,
               std::unique_ptr<const base::i18n::Transliterator>>&
GetTransliteratorsMap() {
  // The `ICU` library does not cache the transliterators created from rules,
  // since their creation caused ANR errors on IOS and Android, it is important
  // that until those are converted to be generated during the compile time,
  // they are cached in memory for the duration of the browser lifetime.
  static base::NoDestructor<base::flat_map<
      TransliterationId, std::unique_ptr<const base::i18n::Transliterator>>>
      autofill_transliterators;
  return *autofill_transliterators;
}

std::unique_ptr<base::i18n::Transliterator> CreateTransliterator(
    TransliterationId id) {
  // Address normalization rules live in
  // `//tools/crates/icu4x_datagen/config.toml`,
  // `//third_party/rust/icu4x_data/transliterators/de_normalize_for_address.txt`,
  // and `//base/i18n/transliterator.cc`:
  // - `und-t-und-x0-afillde` first expands German umlauts per DIN 5007-2
  //   ("ö" -> "oe", "ä" -> "ae", "ü" -> "ue").
  // - `und-t-und-x0-afill` (and the remainder of `und-t-und-x0-afillde`) runs
  //   "::NFD; ::[:Nonspacing Mark:] Remove; ::Lower; ::NFC; ::Latin-ASCII;"
  //   to decompose, strip combining marks, lowercase, recompose, and fold
  //   remaining Latin characters (e.g. "ł" -> "l", "ß" -> "ss") to ASCII.
  switch (id) {
    case TransliterationId::kKatakanaToHiragana:
      return base::i18n::CreateTransliterator("Katakana-Hiragana");
    case TransliterationId::kHiraganaToKatakana:
      return base::i18n::CreateTransliterator("Hiragana-Katakana");
    case TransliterationId::kGerman:
      return base::i18n::CreateTransliterator("und-t-und-x0-afillde");
    case TransliterationId::kDefault:
      return base::i18n::CreateTransliterator("und-t-und-x0-afill");
  }
}

// May return nullptr if the transliterator cannot be initialized.
const base::i18n::Transliterator* GetCachedTransliterator(
    TransliterationId transliteration_id) {
  static base::NoDestructor<base::Lock> getting_transliterator_lock;
  base::AutoLock lock(*getting_transliterator_lock);

  const auto [it, inserted] =
      GetTransliteratorsMap().try_emplace(transliteration_id, nullptr);
  if (inserted) {
    it->second = CreateTransliterator(transliteration_id);
  }
  return it->second.get();
}

std::u16string Transliterate(std::u16string_view value,
                             TransliterationId transliteration_id) {
  if (value.empty()) {
    return std::u16string(value);
  }

  const base::i18n::Transliterator* transliterator =
      GetCachedTransliterator(transliteration_id);
  // Transliterator initialization failed.
  if (!transliterator) {
    return base::i18n::ToLower(value);
  }
  return transliterator->Transliterate(value);
}
}  // namespace

std::u16string RemoveDiacriticsAndConvertToLowerCase(
    std::u16string_view value,
    const AddressCountryCode& country_code) {
  TransliterationId transliteration_id =
      kCountriesWithGermanTransliteration.contains(country_code.value()) &&
              base::FeatureList::IsEnabled(
                  features::kAutofillEnableGermanTransliteration)
          ? TransliterationId::kGerman
          : TransliterationId::kDefault;
  return Transliterate(value, transliteration_id);
}

std::u16string TransliterateAlternativeName(std::u16string_view value,
                                            bool inverse_transliteration) {
  return Transliterate(value, inverse_transliteration
                                  ? TransliterationId::kHiraganaToKatakana
                                  : TransliterationId::kKatakanaToHiragana);
}

// Should be only used for testing. In general transliterators shouldn't be
// deleted during the lifetime of the browser.
void ClearCachedTransliterators() {
  GetTransliteratorsMap().clear();
}

}  // namespace autofill
