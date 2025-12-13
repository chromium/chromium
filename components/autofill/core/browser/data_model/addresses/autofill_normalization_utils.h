// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_NORMALIZATION_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_NORMALIZATION_UTILS_H_

#include <optional>
#include <string_view>

#include "base/i18n/char_iterator.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill::normalization {

// Specifies how punctuation and whitespace should be handled.
enum class WhitespaceSpec { kRetain, kDiscard };

// Iterator for a string that processes punctuation and white space according to
// `collapse_skippable_`.
class NormalizingIterator {
 public:
  NormalizingIterator(std::u16string_view text, WhitespaceSpec whitespace_spec);
  ~NormalizingIterator();

  // Advances to the next non-skippable character in the string. Whether a
  // punctuation or white space character is skippable depends on
  // `collapse_skippable_`. Returns false if the end of the string has been
  // reached.
  void Advance();

  // Returns true if the iterator has reached the end of the string.
  bool End();

  // Returns true if the iterator ends in skippable characters or if the
  // iterator has reached the end of the string. Has the side effect of
  // advancing the iterator to either the first skippable character or to the
  // end of the string.
  bool EndsInSkippableCharacters();

  // Returns the next character that should be considered.
  int32_t GetNextChar();

 private:
  // When `collapse_skippable_` is false, this member is initialized to false
  // and is not updated.
  //
  // When `collapse_skippable_` is true, this member indicates whether the
  // previous character was punctuation or white space so that one or more
  // consecutive embedded punctuation and white space characters can be
  // collapsed to a single white space.
  bool previous_was_skippable_ = false;

  // True if punctuation and white space within the string should be collapsed
  // to a single white space.
  bool collapse_skippable_;

  base::i18n::UTF16CharIterator iter_;
};

// Returns true if `text` is empty or contains only skippable characters. A
// character is skippable if it is punctuation or white space.
bool HasOnlySkippableCharacters(std::u16string_view text);

// Returns a copy of `text` with uppercase converted to lowercase and
// diacritics are rewritten using rules for given `country_code`.
//
// If `whitespace_spec` is kRetain, punctuation is converted to
// spaces, and extraneous whitespace is trimmed and collapsed. For example,
// "Jean- François" becomes "jean francois".
//
// If `whitespace_spec` is kDiscard, punctuation and whitespace are discarded.
// For example, +1 (234) 567-8900 becomes 12345678900.
std::u16string NormalizeForComparison(
    std::u16string_view text,
    WhitespaceSpec whitespace_spec = WhitespaceSpec::kRetain,
    const AddressCountryCode& country_code = AddressCountryCode(""));
}  // namespace autofill::normalization

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_NORMALIZATION_UTILS_H_
