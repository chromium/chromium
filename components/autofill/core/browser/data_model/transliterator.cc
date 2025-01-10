// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/transliterator.h"

#include <memory>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/address.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

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

std::unique_ptr<icu::Transliterator> GetTransliterator(TransliterationId id,
                                                       UErrorCode& err) {
  icu::UnicodeString transliteration_rules;
  UParseError parse_error;
  icu::Transliterator* transliterator;

  switch (id) {
    case TransliterationId::kKatakanaToHiragana:
      transliterator = icu::Transliterator::createInstance("Katakana-Hiragana",
                                                           UTRANS_FORWARD, err);
      break;
    case TransliterationId::kHiraganaToKatakana:
      transliterator = icu::Transliterator::createInstance("Hiragana-Katakana",
                                                           UTRANS_FORWARD, err);
      break;
    case TransliterationId::kGerman:
      // Apply a simplified version of the "::de-ASCII" transliteration, which
      // follows DIN 5007-2 ("ö" becomes "oe"). Here we map everything to
      // lower case because that happens with "::Lower" anyway.
      transliteration_rules = icu::UnicodeString(
          "[ö {o \u0308} Ö {O \u0308}] → oe;"
          "[ä {a \u0308} Ä {A \u0308}] → ae;"
          "[ü {u \u0308} Ü {U \u0308}] → ue;");
      [[fallthrough]];
    case TransliterationId::kDefault:
      // This rules are happening in the following order:
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
      transliterator = icu::Transliterator::createFromRules(
          "NormalizeForAddresses", transliteration_rules, UTRANS_FORWARD,
          parse_error, err);
      break;
  }

  if (U_FAILURE(err) || transliterator == nullptr) {
    // TODO(crbug.com/328968064): Add a histogram to count how often this
    // happens.
    LOG(ERROR) << "Failed to create ICU Transliterator: " << u_errorName(err);
    return nullptr;
  }

  return base::WrapUnique(transliterator);
}
}  // namespace

std::u16string RemoveDiacriticsAndConvertToLowerCase(
    std::u16string_view value,
    const AddressCountryCode& country_code) {
  if (value.empty()) {
    return std::u16string(value);
  }

  UErrorCode err = U_ZERO_ERROR;
  TransliterationId transliteration_id =
      kCountriesWithGermanTransliteration.contains(country_code.value()) &&
              base::FeatureList::IsEnabled(
                  features::kAutofillEnableGermanTransliteration)
          ? TransliterationId::kGerman
          : TransliterationId::kDefault;
  std::unique_ptr<icu::Transliterator> transliterator =
      GetTransliterator(transliteration_id, err);

  if (U_FAILURE(err) || !transliterator) {
    return base::i18n::ToLower(value);
  }
  icu::UnicodeString transliterated_value(
      icu::UnicodeString(value.data(), value.length()));
  transliterator->transliterate(transliterated_value);
  return base::i18n::UnicodeStringToString16(transliterated_value);
}

std::u16string TransliterateAlternativeName(std::u16string_view value,
                                            bool inverse_transliteration) {
  if (value.empty()) {
    return std::u16string(value);
  }

  UErrorCode err = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> transliterator = GetTransliterator(
      inverse_transliteration ? TransliterationId::kHiraganaToKatakana
                              : TransliterationId::kKatakanaToHiragana,
      err);

  if (U_FAILURE(err) || !transliterator) {
    // TODO(crbug.com/359768803): Remove the metric recording once we confirm
    // that transliteration initialization never fails.
    // This metric records the status of the transliterator initialization. It
    // is set to false if the initialization fails.
    base::UmaHistogramBoolean(
        "Autofill.Filling.AlternativeNameTransliteratorInitStatus", false);
    return base::i18n::ToLower(value);
  }
  icu::UnicodeString transliterated_value(
      icu::UnicodeString(value.data(), value.length()));
  transliterator->transliterate(transliterated_value);
  // The metric is set to true if the transliterator initialization was
  // successful.
  base::UmaHistogramBoolean(
      "Autofill.Filling.AlternativeNameTransliteratorInitStatus", true);
  return base::i18n::UnicodeStringToString16(transliterated_value);
}

}  // namespace autofill
