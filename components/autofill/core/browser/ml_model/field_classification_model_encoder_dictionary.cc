// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_encoder_dictionary.h"

#include <algorithm>
#include <vector>

#include "base/check.h"

namespace autofill {

FieldClassificationModelEncoderDictionary::
    FieldClassificationModelEncoderDictionary(
        const google::protobuf::RepeatedPtrField<std::string>& tokens) {
  // Compute total size to reserve string buffer.
  size_t total_size = 0;
  for (const std::string& token : tokens) {
    total_size += token.size();
  }
  dictionary_.reserve(total_size);

  // There are 2 special tokens: `kPaddingTokenId` and `kUnknownTokenId`.
  const size_t num_tokens = tokens.size() + 2;

  // `offsets_` contains an entry with the start position of each token, plus an
  // extra one for the position after the last token.
  offsets_.reserve(num_tokens + 1);

  // Token 0: Padding
  offsets_.push_back(0);
  // Token 1: Unknown
  offsets_.push_back(0);
  // Token 2: First real token
  offsets_.push_back(0);

  match_ids_.reserve(tokens.size());

  uint32_t current_id = 2;
  for (const std::string& token : tokens) {
    dictionary_.append(token);
    offsets_.push_back(dictionary_.size());
    match_ids_.push_back(TokenId(current_id));
    ++current_id;
  }

  // Sort `match_ids_` based on the string contents in `dictionary_`.
  std::ranges::sort(match_ids_, {},
                    [&](TokenId id) { return FindTokenById(id); });
}

FieldClassificationModelEncoderDictionary::
    FieldClassificationModelEncoderDictionary(
        const FieldClassificationModelEncoderDictionary&) = default;
FieldClassificationModelEncoderDictionary::
    FieldClassificationModelEncoderDictionary(
        FieldClassificationModelEncoderDictionary&&) = default;
FieldClassificationModelEncoderDictionary&
FieldClassificationModelEncoderDictionary::operator=(
    const FieldClassificationModelEncoderDictionary&) = default;
FieldClassificationModelEncoderDictionary&
FieldClassificationModelEncoderDictionary::operator=(
    FieldClassificationModelEncoderDictionary&&) = default;

FieldClassificationModelEncoderDictionary::
    ~FieldClassificationModelEncoderDictionary() = default;

FieldClassificationModelEncoderDictionary::TokenId
FieldClassificationModelEncoderDictionary::TokenToId(
    std::string_view token) const {
  if (token.empty()) {
    return kPaddingTokenId;
  }

  auto it = std::ranges::lower_bound(
      match_ids_, token, {}, [&](TokenId id) { return FindTokenById(id); });

  if (it != match_ids_.end() && FindTokenById(*it) == token) {
    return *it;
  }

  return kUnknownTokenId;
}

std::string_view FieldClassificationModelEncoderDictionary::FindTokenById(
    TokenId id) const {
  // `offsets_` is populated with at least 3 elements during construction.
  CHECK(!offsets_.empty());
  if (id.value() >= offsets_.size() - 1) {
    return "";
  }
  return std::string_view(dictionary_)
      .substr(offsets_[id.value()],
              offsets_[id.value() + 1] - offsets_[id.value()]);
}

}  // namespace autofill
