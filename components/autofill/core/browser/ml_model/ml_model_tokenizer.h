// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_ML_MODEL_TOKENIZER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_ML_MODEL_TOKENIZER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/types/strong_alias.h"

namespace autofill {

// The tokenizer performs tokenization for on device autofill field type
// prediction ML model. It maps raw strings to tokens, and tokens to
// IDs accepted by the ML model based on the given dictionary file.
// Empty strings map to value 0 and unknown words map to value 1.
class AutofillMLModelTokenizer {
 public:
  using TokenId = base::StrongAlias<class TokenIdTag, uint32_t>;

  // The number of entries in the output array which will be used in padding.
  static constexpr size_t kOutputSequenceLength = 5;
  // Special characters to remove from the field label input.
  static constexpr char16_t kSpecialChars[] =
      uR"(!"#$%&()\*+,-./:;<=>?@[]^_`{|}~')";

  // Factory function returns instance of the tokenizer if initialized.
  // If dictionary file path is not found, initialization fails and
  // a nullptr is returned instead.
  static std::unique_ptr<AutofillMLModelTokenizer> CreateTokenizer(
      const base::FilePath& dictionary_filepath);

  ~AutofillMLModelTokenizer();

  // Standardize the field label by changing it lower case and stripping
  // punctuation. Then vectorize by splitting it into substrings split by
  // whitespaces, tokenizing each string and padding the array to have
  // size `kOutputSequenceLength`.
  std::array<TokenId, kOutputSequenceLength> Vectorize(
      std::u16string_view input) const;
  TokenId TokenToId(std::u16string_view token) const;

  size_t GetDictionarySize() const;

 private:
  explicit AutofillMLModelTokenizer(
      std::vector<std::pair<std::u16string, TokenId>> entries);
  AutofillMLModelTokenizer(const AutofillMLModelTokenizer& tokenizer);

  const base::flat_map<std::u16string, TokenId> token_to_id_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_ML_MODEL_TOKENIZER_H_
