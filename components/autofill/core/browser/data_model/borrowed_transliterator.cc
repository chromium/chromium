// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"

#include "base/logging.h"
#include "base/no_destructor.h"

namespace autofill {

BorrowedTransliterator::BorrowedTransliterator() : auto_lock_(GetLock()) {}

BorrowedTransliterator::~BorrowedTransliterator() = default;

void BorrowedTransliterator::Transliterate(icu::UnicodeString* text) const {
  if (GetTransliterator() != nullptr) {
    GetTransliterator()->transliterate(*text);
  } else {
    *text = text->toLower();
  }
}

// static
base::Lock& BorrowedTransliterator::GetLock() {
  static base::NoDestructor<base::Lock> instance;
  return *instance;
}

// static
std::unique_ptr<icu::Transliterator>&
BorrowedTransliterator::GetTransliterator() {
  static base::NoDestructor<std::unique_ptr<icu::Transliterator>> instance([] {
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
    //
    // It would be interesting to transliterate umlauts according to DIN 5007-2
    // (::de-ASCII) for German addresses ("ö" to "oe"), but that does not match
    // our assumptions from
    // components/autofill/core/browser/geo/address_rewrite_rules/.
    icu::UnicodeString transliteration_rules =
        "::NFD; ::[:Nonspacing Mark:] Remove; ::Lower; ::NFC; ::Latin-ASCII;";
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
  }());
  return *instance;
}

std::u16string RemoveDiacriticsAndConvertToLowerCase(
    base::StringPiece16 value) {
  icu::UnicodeString result = icu::UnicodeString(value.data(), value.length());
  BorrowedTransliterator().Transliterate(&result);
  return base::i18n::UnicodeStringToString16(result);
}

}  // namespace autofill
