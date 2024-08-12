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
class FormStructure;

// The Encoder performs vectorization for on-device Autofill field type
// prediction ML model. It changes the string input for preprocessing by
// standardizing and tokenizing it. Tokenization maps raw strings to tokens,
// and tokens to IDs based on the given dictionary. Empty Strings map to
// value 0 and unknown words map to value 1.
class AutofillModelEncoder {
 public:
  // The ML model utilizes label, placeholder, and autocomplete attributes for
  // field analysis. Token IDs start at 1 due to a shift during vocabulary
  // loading from model metadata. The label-specific identifier is further
  // incremented by 1 at the end and accordingly for other attributes.
  enum class FieldAttributeIdentifier {
    kLabel = 1,
    kPlaceholder = 2,
    kAutocomplete = 3,
    kMaxValue = kAutocomplete,
  };

  // Maximum number of form fields for which the model can predict types.
  // When calling the executor with a larger form, predictions are only returned
  // for the first `kModelMaxNumberOfFields` many fields.
  static constexpr size_t kModelMaxNumberOfFields = 30;

  // The number of entries in the output array which will be used in padding
  // for the specific one attribute of the field.
  static constexpr size_t kAttributeOutputSequenceLength = 6;

  static constexpr size_t kOutputSequenceLength =
      kAttributeOutputSequenceLength *
      static_cast<size_t>(FieldAttributeIdentifier::kMaxValue);

  // Special characters to remove from the field label input.
  static constexpr char16_t kSpecialChars[] =
      uR"(!"#$%&()\*+,-./:;<=>?@[\]^_`{|}~'×—•∗…–“▼)";

  // Whitespace and separator characters.
  static constexpr char16_t kWhitespaceChars[] =
      u" \xa0\u200b\u3164\u2062\u2063";

  using TokenId = base::StrongAlias<class TokenIdTag, uint32_t>;

  // An encoded representation of the form's labels.
  // Each element of the vector corresponds to an encoded label. See
  // `AutofillModelEncoder`,
  using ModelInput = std::vector<std::array<TokenId, kOutputSequenceLength>>;

  // The model always returns predictions for `kModelMaxNumberOfFields`.
  // If the queried form was smaller, the last
  // (kModelMaxNumberOfFields - fields) elements of the output have
  // unspecified values.
  // The other indices contain a vector with one entry per supported FieldType,
  // representing the confidence in that type. The confidences don't have any
  // meaning, but higher means more confidence. Since the model might not
  // support all FieldTypes, the indices don't map to field types directly. See
  // `AutofillMlPredictionModelHandler`.
  using ModelOutput = std::array<std::vector<float>, kModelMaxNumberOfFields>;

  explicit AutofillModelEncoder(
      const google::protobuf::RepeatedPtrField<std::string>& tokens);

  AutofillModelEncoder();
  AutofillModelEncoder(const AutofillModelEncoder&);
  ~AutofillModelEncoder();

  TokenId TokenToId(std::u16string_view token) const;

  // Tokenize the specific field attribute to the array of size
  // `kAttributeOutputSequenceLength - 1`. The first token is reserved for the
  // identifier which will be applied after encoding the attribute.
  std::array<TokenId, kAttributeOutputSequenceLength - 1> TokenizeAttribute(
      std::u16string_view input) const;

  // Encodes the `form` into the `ModelInput` (std::vector<std::array<TokenId,
  // kOutputSequenceLength>>) representation understood by the
  // `AutofillModelExecutor`. This is done by encoding the attributes of the
  // form's fields.
  ModelInput EncodeForm(const FormStructure& form) const;

  // Constructs from `field` the input for Autofill ML model using field
  // attributes. More specifically, handles the attributes encoding and prepares
  // the final input.
  std::array<TokenId, kOutputSequenceLength> EncodeField(
      const AutofillField& field) const;

  // Standardizes the specific field attribute and pre-pads
  // the array to have the size `kAttributeOutputSequenceLength`.
  std::array<TokenId, kAttributeOutputSequenceLength> EncodeAttribute(
      std::u16string_view input,
      FieldAttributeIdentifier attribute_identifier) const;

  // Converts the attribute identifier to the numeric value.
  TokenId EncodeAttributeIdentifier(
      FieldAttributeIdentifier attribute_identifier) const {
    return TokenId(token_to_id_.size() +
                   static_cast<int>(attribute_identifier));
  }

 private:
  base::flat_map<std::u16string, TokenId> token_to_id_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_ENCODER_H_
