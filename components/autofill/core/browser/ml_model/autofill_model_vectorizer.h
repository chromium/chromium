// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_VECTORIZER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_VECTORIZER_H_

#include <stdint.h>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/types/strong_alias.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace autofill {

// The Vectorizer performs vectorization for on-device Autofill field type
// prediction ML model. It changes the string input for preprocessing by
// standardizing and tokenizing it. Tokenization maps raw strings to tokens,
// and tokens to IDs based on the given dictionary. Empty Strings map to
// value 0 and unknown words map to value 1.
class AutofillModelVectorizer {
 public:
  using TokenId = base::StrongAlias<class TokenIdTag, uint32_t>;

  // The number of entries in the output array which will be used in padding.
  static constexpr size_t kOutputSequenceLength = 5;
  // Special characters to remove from the field label input.
  static constexpr char16_t kSpecialChars[] =
      uR"(!"#$%&()\*+,-./:;<=>?@[]^_`{|}~')";

  explicit AutofillModelVectorizer(
      const google::protobuf::RepeatedPtrField<std::string>& tokens);

  AutofillModelVectorizer();
  AutofillModelVectorizer(const AutofillModelVectorizer&);
  ~AutofillModelVectorizer();

  // Standardize the field label by changing it lower case and stripping
  // punctuation. Then vectorize by splitting it into substrings split by
  // whitespaces, tokenizing each string and padding the array to have
  // size `kOutputSequenceLength`.
  std::array<TokenId, kOutputSequenceLength> Vectorize(
      std::u16string_view input) const;
  TokenId TokenToId(std::u16string_view token) const;

 private:
  base::flat_map<std::u16string, TokenId> token_to_id_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_VECTORIZER_H_
