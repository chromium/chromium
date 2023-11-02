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
std::unique_ptr<icu::Transliterator>
BorrowedTransliterator::CreateTransliterator() {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Transliterator> transliterator(
      icu::Transliterator::createInstance(
          "NFD; [:Nonspacing Mark:] Remove; Lower; NFC", UTRANS_FORWARD,
          status));
  if (U_FAILURE(status) || transliterator == nullptr) {
    // TODO(rogerm): Add a histogram to count how often this happens.
    LOG(ERROR) << "Failed to create ICU Transliterator: "
               << u_errorName(status);
  }
  return transliterator;
}

// static
std::unique_ptr<icu::Transliterator>&
BorrowedTransliterator::GetTransliterator() {
  static base::NoDestructor<std::unique_ptr<icu::Transliterator>> instance(
      CreateTransliterator());
  return *instance;
}

std::u16string RemoveDiacriticsAndConvertToLowerCase(
    base::StringPiece16 value) {
  icu::UnicodeString result = icu::UnicodeString(value.data(), value.length());
  BorrowedTransliterator().Transliterate(&result);
  return base::i18n::UnicodeStringToString16(result);
}

}  // namespace autofill
