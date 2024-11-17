// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_ENCODER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_ENCODER_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/types/strong_alias.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace autofill {

class AutofillField;
class FormStructure;

// The Encoder performs vectorization for on-device field type prediction ML
// model. It changes the string input for preprocessing by standardizing and
// tokenizing it. Tokenization maps raw strings to tokens, and tokens to IDs
// based on the given dictionary. Empty Strings map to value 0 and unknown words
// map to value 1.
class FieldClassificationModelEncoder {
 public:
  using TokenId = base::StrongAlias<class TokenIdTag, uint32_t>;

  // An encoded representation of the form's labels.
  // Each element of the vector corresponds to an encoded feature (e.g. HTML
  // attribute or an extracted label). See `FieldClassificationModelEncoder`.
  // Dimensionality: (#fields, 1 + len(features) * max_tokens_per_feature)
  // where max_tokens_per_feature and features are attributes of
  // encoding_parameters_. The "1" originates from a prepended CLS token.
  using ModelInput = std::vector<std::vector<TokenId>>;

  // The model always returns predictions for
  // `encoding_parameters_.maximum_number_of_fields`.
  // If the queried form was smaller, the last
  // (maximum_number_of_fields - fields) elements of the output have
  // unspecified values.
  // model_output[i] contains a vector with one entry per supported FieldType,
  // representing the confidence in that type. The confidences don't have any
  // meaning, but higher means more confidence. Since the model might not
  // support all FieldTypes, the indices don't map to field types directly. See
  // `FieldClassificationModelHandler`.
  // Dimensionality: (maximum_number_of_fields, len(output_type))
  // where output_type is an attribute of the
  // `AutofillFieldClassificationModelMetadata`.
  using ModelOutput = std::vector<std::vector<float>>;

  explicit FieldClassificationModelEncoder(
      const google::protobuf::RepeatedPtrField<std::string>& tokens,
      optimization_guide::proto::AutofillFieldClassificationEncodingParameters
          encoding_parameters);

  FieldClassificationModelEncoder();
  FieldClassificationModelEncoder(const FieldClassificationModelEncoder&);
  ~FieldClassificationModelEncoder();

  TokenId TokenToId(std::u16string_view token) const;

  // Encodes the `form` into the `ModelInput` representation understood by the
  // `FieldClassificationModelExecutor`. This is done by encoding the attributes
  // of the form's fields.
  ModelInput EncodeForm(const FormStructure& form) const;

  // Constructs from `field` the input for Field Classification ML model using
  // field attributes. More specifically, handles the attributes encoding and
  // prepares the final input.
  std::vector<TokenId> EncodeField(const AutofillField& field) const;

  // Constructs the input for Field Classification ML model using
  // form level attributes.
  std::vector<FieldClassificationModelEncoder::TokenId> EncodeFormFeatures(
      const FormStructure& form) const;

  // Standardizes a string according to encoding_parameters_:
  //   - Optionally split on CamelCase.
  //   - Optionally map to lowercase.
  //   - Replace specified characters with whitespace.
  //   - Remove specified characters.
  std::u16string StandardizeString(std::u16string_view input) const;

  // Tokenizes the specific `input` to a vector of size
  // `max_tokens_per_feature`. The token IDs are looked up in token_to_id_
  // after standardizing and splitting on whitespace.
  // Excess tokens are deleted or extra tokens (representing empty
  // strings) are appended to generate a vector of the desired size.
  std::vector<TokenId> EncodeAttribute(std::u16string_view input) const;

 private:
  // Returns if the model's encoding parameters specify any form level features.
  bool ShouldEncodeFormLevelFeatures() const;

  // Returns a special CLS ("classification") token indicating the beginning of
  // a field. This is the token where the encoder generates the field
  // classification in the output.
  TokenId cls_token() const { return TokenId(token_to_id_.size() + 1); }

  // Returns a special CLS ("classification") token indicating the beginning of
  // a placeholder field containing information about the form-level features.
  // Only used when the model is requiring encoding form level features.
  TokenId form_cls_token() const;

  base::flat_map<std::u16string, TokenId> token_to_id_;
  optimization_guide::proto::AutofillFieldClassificationEncodingParameters
      encoding_parameters_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_ENCODER_H_
