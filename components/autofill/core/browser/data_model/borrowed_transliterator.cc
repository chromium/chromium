// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

BorrowedTransliterator::BorrowedTransliterator() : auto_lock_(GetLock()) {}

BorrowedTransliterator::~BorrowedTransliterator() = default;

void BorrowedTransliterator::Transliterate(
    icu::UnicodeString& text,
    AddressCountryCode country_code) const {
  if (GetTransliterator(country_code) != nullptr) {
    GetTransliterator(country_code)->transliterate(text);
  } else {
    text = text.toLower();
  }
}

// static
base::Lock& BorrowedTransliterator::GetLock() {
  static base::NoDestructor<base::Lock> instance;
  return *instance;
}

// static
std::unique_ptr<icu::Transliterator>& BorrowedTransliterator::GetTransliterator(
    const AddressCountryCode& country_code) const {
  // Apply German transliteration according to DIN 5007-2 in these countries.
  // "ö" becomes "oe" instead of "o."
  static constexpr auto kCountriesWithGermanTransliteration =
      base::MakeFixedFlatSet<std::string_view>(
          {"AT", "BE", "CH", "DE", "IT", "LI", "LU"});

  auto create_transliteration = [](bool apply_german_transliteration) {
    UErrorCode status = U_ZERO_ERROR;
    UParseError parse_error;
    // This is happening in the following rule:
    // "::NFD;" performs a decomposition and normalization. (â becomes a and  ̂)
    // "::[:Nonspacing Mark:] Remove;" removes the " ̂"
    // "::Lower;" converts the result to lower case
    // "::NFC;" re-composes the decomposed characters
    // "::Latin-ASCII;" converts various other Latin characters to an ASCII
    //   representation (e.g. "ł", which does not get decomposed, to "l"; "ß" to
    //   "ss").

    icu::UnicodeString transliteration_rules =
        "::NFD; ::[:Nonspacing Mark:] Remove; ::Lower; ::NFC; ::Latin-ASCII;";

    if (apply_german_transliteration) {
      // Apply a simplified version of the "::de-ASCII" transliteration, which
      // follows DIN 5007-2 ("ö" becomes "oe"). Here we map everything to
      // lower case because that happens with "::Lower" anyway.
      transliteration_rules = icu::UnicodeString(
                                  "[ö {o \u0308} Ö {O \u0308}] → oe;"
                                  "[ä {a \u0308} Ä {A \u0308}] → ae;"
                                  "[ü {u \u0308} Ü {U \u0308}] → ue;") +
                              transliteration_rules;
    }

    std::unique_ptr<icu::Transliterator> transliterator(
        icu::Transliterator::createFromRules(
            "NormalizeForAddresses", transliteration_rules, UTRANS_FORWARD,
            parse_error, status));
    if (U_FAILURE(status) || transliterator == nullptr) {
      // TODO(rogerm): Add a histogram to count how often this happens.
      LOG(ERROR) << "Failed to create ICU Transliterator: "
                 << u_errorName(status);
    }
    return transliterator;
  };

  if (kCountriesWithGermanTransliteration.contains(country_code.value()) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableGermanTransliteration)) {
    static base::NoDestructor<std::unique_ptr<icu::Transliterator>> instance(
        create_transliteration(/*apply_german_transliteration=*/true));
    return *instance;
  }
  static base::NoDestructor<std::unique_ptr<icu::Transliterator>> instance(
      create_transliteration(/*apply_german_transliteration=*/false));
  return *instance;
}

std::u16string RemoveDiacriticsAndConvertToLowerCase(
    std::u16string_view value,
    const AddressCountryCode& country_code) {
  icu::UnicodeString result = icu::UnicodeString(value.data(), value.length());
  BorrowedTransliterator().Transliterate(result, country_code);
  return base::i18n::UnicodeStringToString16(result);
}

}  // namespace autofill
