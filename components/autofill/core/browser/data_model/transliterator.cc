// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/transliterator.h"

#include <memory>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/transliterator.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/address.h"
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
  std::string transliteration_rules;
  std::unique_ptr<base::i18n::Transliterator> transliterator;

  switch (id) {
    case TransliterationId::kKatakanaToHiragana:
      transliterator = base::i18n::CreateTransliterator("Katakana-Hiragana");
      break;
    case TransliterationId::kHiraganaToKatakana:
      transliterator = base::i18n::CreateTransliterator("Hiragana-Katakana");
      break;
    case TransliterationId::kGerman:
      // Apply a simplified version of the "::de-ASCII" transliteration, which
      // follows DIN 5007-2 ("ö" becomes "oe"). Here we map everything to
      // lower case because that happens with "::Lower" anyway.
      transliteration_rules =
          "[ö {o \u0308} Ö {O \u0308}] → oe;"
          "[ä {a \u0308} Ä {A \u0308}] → ae;"
          "[ü {u \u0308} Ü {U \u0308}] → ue;";
      [[fallthrough]];
    case TransliterationId::kDefault:
      // These rules are happening in the following order:
      // First there are `TransliterationId::kGerman` specific rules if they are
      // present, then
      // "::NFD;" performs a decomposition and normalization.
      // (â becomes a and ̂)
      // "::[:Nonspacing Mark:] Remove;" removes the " ̂"
      // "::Lower;" converts the result to lower case
      // "::NFC;" re-composes the decomposed characters
      // "::Latin-ASCII;" converts various other Latin characters to an ASCII
      //   representation (e.g. "ł", which does not get decomposed, to "l"; "ß"
      //   to "ss").
      transliteration_rules +=
          "::NFD; ::[:Nonspacing Mark:] Remove; ::Lower; ::NFC; ::Latin-ASCII;";
      transliterator = base::i18n::CreateTransliteratorFromRules(
          "NormalizeForAddresses", transliteration_rules);
      break;
  }

  if (!transliterator) {
    base::UmaHistogramBoolean("Autofill.TransliteratorInitStatus", false);
    return nullptr;
  }

  base::UmaHistogramBoolean("Autofill.TransliteratorInitStatus", true);
  return transliterator;
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

  base::Time transliterator_creation_time = base::Time::Now();
  const base::i18n::Transliterator* transliterator =
      GetCachedTransliterator(transliteration_id);
  // TODO(crbug.com/399657187): Remove once the issue is resolved.
  base::UmaHistogramTimes("Autofill.TransliteratorCreationTime",
                          base::Time::Now() - transliterator_creation_time);

  // Transliterator initialization failed.
  if (!transliterator) {
    return base::i18n::ToLower(value);
  }
  base::ScopedUmaHistogramTimer logger("Autofill.TransliterationDuration");
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
