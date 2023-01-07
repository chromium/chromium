// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BORROWED_TRANSLITERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BORROWED_TRANSLITERATOR_H_

#include "base/i18n/unicodestring.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/translit.h"

namespace autofill {

// This RAII class provides a thread-safe interface to a shared transliterator.
// Sharing a single transliterator is advisable due its high construction cost.
class BorrowedTransliterator {
 public:
  BorrowedTransliterator();
  virtual ~BorrowedTransliterator();

  void Transliterate(icu::UnicodeString* text) const;

 private:
  static base::Lock& GetLock();

  // Use ICU transliteration to remove diacritics and fold case.
  // See http://userguide.icu-project.org/transforms/general
  static std::unique_ptr<icu::Transliterator> CreateTransliterator();

  static std::unique_ptr<icu::Transliterator>& GetTransliterator();

  base::AutoLock auto_lock_;
};

// Apply the transliteration to a full string to convert it to lower case and to
// remove the diacritics.
// and remove the diacritics.
std::u16string RemoveDiacriticsAndConvertToLowerCase(base::StringPiece16 value);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BORROWED_TRANSLITERATOR_H_
