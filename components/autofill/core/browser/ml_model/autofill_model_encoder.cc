// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_encoder.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace autofill {

namespace {

constexpr AutofillModelEncoder::TokenId kUnknownTokenId =
    AutofillModelEncoder::TokenId(1);

}  // namespace

AutofillModelEncoder::AutofillModelEncoder(
    const google::protobuf::RepeatedPtrField<std::string>& tokens,
    optimization_guide::proto::AutofillFieldClassificationEncodingParameters
        encoding_parameters)
    : encoding_parameters_(std::move(encoding_parameters)) {
  std::vector<std::pair<std::u16string, AutofillModelEncoder::TokenId>>
      entries = {
          // Index 0 is reserved for padding to `kOutputSequenceLength`.
          // For example, a label "first name" is encoded as [?, ?, 0] if the
          // output sequence length is 3.
          {u"", AutofillModelEncoder::TokenId(0)},
          // Index 1 is reserved for words not in the dictionary.
          {u"", kUnknownTokenId},
      };
  entries.reserve(2 + tokens.size());
  size_t i = 2;
  for (const std::string& token : tokens) {
    entries.emplace_back(base::UTF8ToUTF16(token), TokenId(i++));
  }
  token_to_id_ = base::flat_map<std::u16string, TokenId>(std::move(entries));
}

AutofillModelEncoder::AutofillModelEncoder() = default;
AutofillModelEncoder::AutofillModelEncoder(const AutofillModelEncoder&) =
    default;
AutofillModelEncoder::~AutofillModelEncoder() = default;

AutofillModelEncoder::TokenId AutofillModelEncoder::TokenToId(
    std::u16string_view token) const {
  auto match = token_to_id_.find(token);
  if (match == token_to_id_.end()) {
    return kUnknownTokenId;
  }
  return match->second;
}

std::vector<std::vector<AutofillModelEncoder::TokenId>>
AutofillModelEncoder::EncodeForm(const FormStructure& form) const {
  std::vector<std::vector<TokenId>> encoded_form(form.field_count());
  for (size_t i = 0; i < form.field_count(); ++i) {
    encoded_form[i] = EncodeField(*form.field(i));
  }
  return encoded_form;
}

std::vector<AutofillModelEncoder::TokenId> AutofillModelEncoder::EncodeField(
    const AutofillField& field) const {
  // Protobuf does not generate an `enum class`. Therefore, this points to the
  // wrapping container class.
  using FeaturesEnum =
      optimization_guide::proto::AutofillFieldClassificationEncodingParameters;

  auto encode = [&](int feature) -> std::vector<TokenId> {
    static_assert(FeaturesEnum::AutofillFieldClassificationFeature_MAX ==
                      FeaturesEnum::FEATURE_NAME,
                  "Update the switch when adding more features");
    switch (feature) {
      case FeaturesEnum::FEATURE_UNKNOWN:
        return {};
      case FeaturesEnum::FEATURE_LABEL:
        return EncodeAttribute(field.label());
      case FeaturesEnum::FEATURE_AUTOCOMPLETE:
        return EncodeAttribute(
            base::UTF8ToUTF16(field.autocomplete_attribute()));
      case FeaturesEnum::FEATURE_PLACEHOLDER:
        return EncodeAttribute(field.placeholder());
      case FeaturesEnum::FEATURE_ID:
        return EncodeAttribute(field.id_attribute());
      case FeaturesEnum::FEATURE_NAME:
        return EncodeAttribute(field.name_attribute());
    }
    return {};
  };
  std::vector<TokenId> output;
  output.reserve(1 + encoding_parameters_.features_size() *
                         encoding_parameters_.max_tokens_per_feature());
  output.emplace_back(cls_token());
  for (int feature : encoding_parameters_.features()) {
    base::ranges::move(encode(feature), std::back_inserter(output));
  }
  return output;
}

std::vector<AutofillModelEncoder::TokenId>
AutofillModelEncoder::EncodeAttribute(std::u16string_view input) const {
  std::u16string standardized_input = base::ToLowerASCII(input);
  base::RemoveChars(standardized_input, kSpecialChars, &standardized_input);
  std::vector<std::u16string> split_string =
      base::SplitString(standardized_input, kWhitespaceChars,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // Padding the output to be of size `max_tokens_per_feature`.
  split_string.resize(encoding_parameters_.max_tokens_per_feature(), u"");

  return base::ToVector(split_string, [&](std::u16string_view token) {
    return TokenToId(token);
  });
}

}  // namespace autofill
