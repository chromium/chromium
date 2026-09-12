// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_ENCODER_DICTIONARY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_ENCODER_DICTIONARY_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/types/strong_alias.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace autofill {

// A memory-efficient data structure for storing the vocabulary dictionary of
// the Field Classification ML model. It packs all string tokens into a single
// contiguous buffer and maintains indices for O(log N) lookups.
class FieldClassificationModelEncoderDictionary {
 public:
  using TokenId = base::StrongAlias<class TokenIdTag, uint32_t>;

  // Token for padding. For example, a label "first name" is encoded as
  // [?, ?, 0] if the output sequence length is 3.
  static constexpr TokenId kPaddingTokenId = TokenId(0);
  // Token for unknown tokens (words not in the vocabulary).
  static constexpr TokenId kUnknownTokenId = TokenId(1);

  explicit FieldClassificationModelEncoderDictionary(
      const google::protobuf::RepeatedPtrField<std::string>& tokens);
  FieldClassificationModelEncoderDictionary(
      const FieldClassificationModelEncoderDictionary&);
  FieldClassificationModelEncoderDictionary(
      FieldClassificationModelEncoderDictionary&&);
  FieldClassificationModelEncoderDictionary& operator=(
      const FieldClassificationModelEncoderDictionary&);
  FieldClassificationModelEncoderDictionary& operator=(
      FieldClassificationModelEncoderDictionary&&);
  ~FieldClassificationModelEncoderDictionary();

  // Returns the TokenId for the given token.
  // Returns `kUnknownTokenId` for unknown tokens (out of vocabulary).
  TokenId TokenToId(std::string_view token) const;

  // Returns the string for a given TokenId. Returns empty string if not found.
  std::string_view FindTokenById(TokenId id) const;

  // Returns the total number of vocabulary elements, including padding and
  // unknown token placeholders (e.g. if the original list from the model has
  // 5 tokens, this will return 7).
  size_t GetVocabularySize() const {
    // `offsets_` is populated with at least 3 elements during construction.
    CHECK(!offsets_.empty());
    return offsets_.size() - 1;
  }

 private:
  // A single contiguous buffer holding all tokens concatenated in UTF-8
  // without null terminators.
  std::string dictionary_;

  // Maps a TokenId `i` to the byte offset in `dictionary_`.
  // The token string starts at `offsets_[i]` and has length
  // `offsets_[i+1] - offsets_[i]`.
  // The array contains one entry per TokenId (including `kPaddingTokenId` and
  // `kUnknownTokenId`), plus a trailing sentinel entry so that the length of
  // the last token can be computed. Its size is therefore
  // `GetVocabularySize() + 1`.
  std::vector<uint32_t> offsets_;

  // A sorted array of TokenIds, ordered alphabetically by their corresponding
  // strings in `dictionary_`. Used exclusively for binary search.
  std::vector<TokenId> match_ids_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_ENCODER_DICTIONARY_H_
