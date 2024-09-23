// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_encoder.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
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
    const google::protobuf::RepeatedPtrField<std::string>& tokens) {
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

std::vector<std::array<AutofillModelEncoder::TokenId,
                       AutofillModelEncoder::kOutputSequenceLength>>
AutofillModelEncoder::EncodeForm(const FormStructure& form) const {
  std::vector<std::array<TokenId, kOutputSequenceLength>> encoded_form(
      form.fields().size());
  for (size_t i = 0; i < form.field_count(); ++i) {
    encoded_form[i] = EncodeField(*form.field(i));
  }
  return encoded_form;
}

std::array<AutofillModelEncoder::TokenId,
           AutofillModelEncoder::kOutputSequenceLength>
AutofillModelEncoder::EncodeField(const AutofillField& field) const {
  using EncodedAttribute = std::array<TokenId, kAttributeOutputSequenceLength>;
  std::vector<EncodedAttribute> encoded_attributes = {
      EncodeAttribute(field.label(), FieldAttributeIdentifier::kLabel),
      EncodeAttribute(base::UTF8ToUTF16(field.autocomplete_attribute()),
                      FieldAttributeIdentifier::kAutocomplete),
      EncodeAttribute(field.placeholder(),
                      FieldAttributeIdentifier::kPlaceholder),
  };

  // Concatenate the encoded attributes to one output of length
  // `kOutputSequenceLength`.
  std::array<TokenId, kOutputSequenceLength> output;
  auto it = output.begin();
  for (EncodedAttribute& encoded_attribute : encoded_attributes) {
    it = base::ranges::move(encoded_attribute, it);
  }
  return output;
}

std::array<AutofillModelEncoder::TokenId,
           AutofillModelEncoder::kAttributeOutputSequenceLength>
AutofillModelEncoder::EncodeAttribute(
    std::u16string_view input,
    FieldAttributeIdentifier attribute_identifier) const {
  std::array<AutofillModelEncoder::TokenId,
             AutofillModelEncoder::kAttributeOutputSequenceLength - 1>
      tokenized_attribute = TokenizeAttribute(input);
  std::array<AutofillModelEncoder::TokenId,
             AutofillModelEncoder::kAttributeOutputSequenceLength>
      output;
  output[0] = EncodeAttributeIdentifier(attribute_identifier);
  base::ranges::move(tokenized_attribute, output.begin() + 1);
  return output;
}

std::array<AutofillModelEncoder::TokenId,
           AutofillModelEncoder::kAttributeOutputSequenceLength - 1>
AutofillModelEncoder::TokenizeAttribute(std::u16string_view input) const {
  std::u16string standardized_input = base::ToLowerASCII(input);
  base::RemoveChars(standardized_input, kSpecialChars, &standardized_input);
  std::vector<std::u16string> split_string =
      base::SplitString(standardized_input, kWhitespaceChars,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // Padding the output to be of size `kAttributeOutputSequenceLength`.
  split_string.resize(kAttributeOutputSequenceLength - 1, u"");
  std::array<TokenId, kAttributeOutputSequenceLength - 1> output;
  base::ranges::transform(
      split_string, output.begin(),
      [&](std::u16string_view token) { return TokenToId(token); });
  return output;
}

}  // namespace autofill
