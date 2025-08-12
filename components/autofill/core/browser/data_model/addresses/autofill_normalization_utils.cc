// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"

#include <string_view>

#include "base/strings/utf_string_conversion_utils.h"
#include "components/autofill/core/browser/data_model/transliterator.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace autofill::normalization {

namespace {

// Returns true if `character` is punctuation or white space.
bool IsPunctuationOrWhitespace(const int8_t character) {
  switch (character) {
    // Punctuation
    case U_DASH_PUNCTUATION:
    case U_START_PUNCTUATION:
    case U_END_PUNCTUATION:
    case U_CONNECTOR_PUNCTUATION:
    case U_OTHER_PUNCTUATION:
    // Whitespace
    case U_CONTROL_CHAR:  // To escape the '\n' character.
    case U_SPACE_SEPARATOR:
    case U_LINE_SEPARATOR:
    case U_PARAGRAPH_SEPARATOR:
      return true;

    default:
      return false;
  }
}

}  // namespace

NormalizingIterator::NormalizingIterator(std::u16string_view text,
                                         WhitespaceSpec whitespace_spec)
    : collapse_skippable_(whitespace_spec == WhitespaceSpec::kRetain),
      iter_(text) {
  int32_t character = iter_.get();

  while (!iter_.end() && IsPunctuationOrWhitespace(u_charType(character))) {
    iter_.Advance();
    character = iter_.get();
  }
}

NormalizingIterator::~NormalizingIterator() = default;

void NormalizingIterator::Advance() {
  if (!iter_.Advance()) {
    return;
  }

  while (!End()) {
    int32_t character = iter_.get();
    bool is_punctuation_or_whitespace =
        IsPunctuationOrWhitespace(u_charType(character));

    if (!is_punctuation_or_whitespace) {
      previous_was_skippable_ = false;
      return;
    }

    if (is_punctuation_or_whitespace && !previous_was_skippable_ &&
        collapse_skippable_) {
      // Punctuation or white space within the string was found, e.g. the "," in
      // the string "Hotel Schmotel, 3 Old Rd", and is after a non-skippable
      // character.
      previous_was_skippable_ = true;
      return;
    }

    iter_.Advance();
  }
}

bool NormalizingIterator::End() {
  return iter_.end();
}

bool NormalizingIterator::EndsInSkippableCharacters() {
  while (!End()) {
    int32_t character = iter_.get();
    if (!IsPunctuationOrWhitespace(u_charType(character))) {
      return false;
    }
    iter_.Advance();
  }
  return true;
}

int32_t NormalizingIterator::GetNextChar() {
  if (End()) {
    return 0;
  }

  if (previous_was_skippable_) {
    return ' ';
  }

  return iter_.get();
}

bool HasOnlySkippableCharacters(std::u16string_view text) {
  if (text.empty()) {
    return true;
  }

  return NormalizingIterator(text, WhitespaceSpec::kDiscard).End();
}

std::u16string NormalizeForComparison(std::u16string_view text,
                                      WhitespaceSpec whitespace_spec,
                                      const AddressCountryCode& country_code) {
  // This algorithm is not designed to be perfect, we could get arbitrarily
  // fancy here trying to canonicalize address lines. Instead, this is designed
  // to handle common cases for all types of data (addresses and names) without
  // needing domain-specific logic.
  //
  // 1. Convert punctuation to spaces and normalize all whitespace to spaces if
  //    `whitespace_spec` is WhitespaceSpec::kRetain.
  //    This will convert "Mid-Island Plz." -> "Mid Island Plz " (the trailing
  //    space will be trimmed off outside of the end of the loop).
  //
  // 2. Collapse consecutive punctuation/whitespace characters to a single
  //    space. We pretend the string has already started with whitespace in
  //    order to trim leading spaces.
  //    If kDiscard was picked, remove all the punctuation/whitespace characters
  //    altogether.
  //
  // 3. Remove diacritics (accents and other non-spacing marks) and perform
  //    case folding to lower-case.
  std::u16string result;
  result.reserve(text.length());
  const bool retain_whitespace = whitespace_spec == WhitespaceSpec::kRetain;
  bool previous_was_whitespace = true;
  for (base::i18n::UTF16CharIterator iter(text); !iter.end(); iter.Advance()) {
    if (!IsPunctuationOrWhitespace(u_charType(iter.get()))) {
      previous_was_whitespace = false;
      base::WriteUnicodeCharacter(iter.get(), &result);
    } else if (retain_whitespace && !previous_was_whitespace) {
      result.push_back(' ');
      previous_was_whitespace = true;
    }
  }
  // Trim off trailing whitespace if we left one.
  if (previous_was_whitespace && !result.empty()) {
    result.resize(result.size() - 1);
  }

  return RemoveDiacriticsAndConvertToLowerCase(result, country_code);
}

}  // namespace autofill::normalization
