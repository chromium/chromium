// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TRANSLITERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TRANSLITERATOR_H_

#include "components/autofill/core/browser/country_type.h"

namespace autofill {

// Applies the transliteration to a full string to convert it to lower case and
// to remove the diacritics. This function also converts other Latin characters
// to ascii (ł -> l, ß -> ss) and applies German transliteration on German
// speaking countries when a `country_code` is provided. Note that the function
// does not apply German transliteration unconditionally because it's incorrect
// in many languages.
std::u16string RemoveDiacriticsAndConvertToLowerCase(
    std::u16string_view value,
    const AddressCountryCode& country_code = AddressCountryCode(""));

// This function transliterates (i.e. converts a string to a semantically the
// same string, but with a different character set) the `value` using the ICU
// library.
// By default the transliteration happens from Katakana to Hiragana, if the
// `inverse_transliteration` is set to true, then the transliteration will
// happen from Hiragana to Katakana.
std::u16string TransliterateAlternativeName(
    std::u16string_view value,
    bool inverse_transliteration = false);

#if defined(UNIT_TEST)
// Clears the stored transliterators, should be only used for testing puproses.
void ClearCachedTransliterators();
#endif

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_TRANSLITERATOR_H_
