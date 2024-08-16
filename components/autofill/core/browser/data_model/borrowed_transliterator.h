// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BORROWED_TRANSLITERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BORROWED_TRANSLITERATOR_H_

#include <string_view>

#include "base/i18n/unicodestring.h"
#include "base/memory/stack_allocated.h"
#include "base/synchronization/lock.h"
#include "components/autofill/core/browser/country_type.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace autofill {

// This RAII class provides a thread-safe interface to a shared transliterator.
// Sharing a single transliterator is advisable due its high construction cost.
class BorrowedTransliterator {
  STACK_ALLOCATED();

 public:
  BorrowedTransliterator();
  virtual ~BorrowedTransliterator();

  // Use ICU transliteration to remove diacritics, fold case and transliterate
  // Latin to ASCII. If a `country_code` is provided, German transliteration is
  // applied on German speaking countries.
  // See http://userguide.icu-project.org/transforms/general
  void Transliterate(
      icu::UnicodeString& text,
      AddressCountryCode country_code = AddressCountryCode("")) const;

 private:
  static base::Lock& GetLock();
  std::unique_ptr<icu::Transliterator>& GetTransliterator(
      const AddressCountryCode& country_code) const;

  base::AutoLock auto_lock_;
};

// Apply the transliteration to a full string to convert it to lower case and to
// remove the diacritics. This function also converts other Latin characters to
// ascii (ł -> l, ß -> ss) and applies German transliteration on German speaking
// countries when a `country_code` is provided. Note that the function does not
// apply German transliteration unconditionally because it's incorrect in many
// languages.
std::u16string RemoveDiacriticsAndConvertToLowerCase(
    std::u16string_view value,
    const AddressCountryCode& country_code = AddressCountryCode(""));

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BORROWED_TRANSLITERATOR_H_
