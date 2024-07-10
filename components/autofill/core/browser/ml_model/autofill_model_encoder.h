// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_ENCODER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_ENCODER_H_

#include <stdint.h>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/types/strong_alias.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace autofill {

class AutofillField;
class AutofillModelExecutor;
class FormStructure;

// The Encoder performs vectorization for on-device Autofill field type
// prediction ML model. It changes the string input for preprocessing by
// standardizing and tokenizing it. Tokenization maps raw strings to tokens,
// and tokens to IDs based on the given dictionary. Empty Strings map to
// value 0 and unknown words map to value 1.
class AutofillModelEncoder {
 public:
  enum class FieldAttributeIdentifier {
    kFieldLabelAttribute = 0,
    kMaxValue = kFieldLabelAttribute,
  };

  using TokenId = base::StrongAlias<class TokenIdTag, uint32_t>;

  // The total number of entries in the output array including all the
  // attributes needed.
  static constexpr size_t kOutputSequenceLength = 5;

  // The number of entries in the output array which will be used in padding
  // for the specific one attribute of the field.
  static constexpr size_t kAttributeOutputSequenceLength = 5;

  static_assert(
      kAttributeOutputSequenceLength *
              (static_cast<size_t>(FieldAttributeIdentifier::kMaxValue) + 1) ==
          kOutputSequenceLength,
      "Number of field attributes used for field encoding doesn't "
      "match `kOutputSequenceLength`");

  // Special characters to remove from the field label input.
  static constexpr char16_t kSpecialChars[] =
      uR"(!"#$%&()\*+,-./:;<=>?@[]^_`{|}~')";

  explicit AutofillModelEncoder(
      const google::protobuf::RepeatedPtrField<std::string>& tokens);

  AutofillModelEncoder();
  AutofillModelEncoder(const AutofillModelEncoder&);
  ~AutofillModelEncoder();

  // Encodes the `form` into the `ModelInput` (std::vector<std::array<TokenId,
  // kOutputSequenceLength>>) representation understood by the
  // `AutofillModelExecutor`. This is done by encoding the attributes of the
  // form's fields.
  std::vector<std::array<TokenId, kOutputSequenceLength>> EncodeForm(
      const FormStructure& form) const;

  // Constructs from `field` the input for Autofill ML model using field
  // attributes. More specifically, handles the attributes encoding and prepares
  // the final input.
  std::array<TokenId, kOutputSequenceLength> EncodeField(
      const AutofillField& field) const;

  // Encode (standardize and tokenize) the specific field attribute and post-pad
  // the array to have the size `kAttributeOutputSequenceLength`.
  std::array<TokenId, kAttributeOutputSequenceLength> EncodeAttribute(
      std::u16string_view input) const;

  TokenId TokenToId(std::u16string_view token) const;

 private:
  base::flat_map<std::u16string, TokenId> token_to_id_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_ENCODER_H_
