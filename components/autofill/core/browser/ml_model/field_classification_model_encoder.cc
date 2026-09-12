// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/i18n/case_conversion.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

namespace {

size_t GetFieldEncodingSize(
    const optimization_guide::proto::
        AutofillFieldClassificationEncodingParameters& encoding_parameters) {
  return 1 + encoding_parameters.max_tokens_per_feature() *
                 std::max(encoding_parameters.features_size(),
                          encoding_parameters.form_features_size());
}

}  // namespace

FieldClassificationModelEncoder::FieldClassificationModelEncoder(
    const google::protobuf::RepeatedPtrField<std::string>& tokens,
    optimization_guide::proto::AutofillFieldClassificationEncodingParameters
        encoding_parameters)
    : dictionary_(tokens),
      encoding_parameters_(std::move(encoding_parameters)) {}

FieldClassificationModelEncoder::FieldClassificationModelEncoder(
    const FieldClassificationModelEncoder&) = default;
FieldClassificationModelEncoder::FieldClassificationModelEncoder(
    FieldClassificationModelEncoder&&) = default;
FieldClassificationModelEncoder& FieldClassificationModelEncoder::operator=(
    const FieldClassificationModelEncoder&) = default;
FieldClassificationModelEncoder& FieldClassificationModelEncoder::operator=(
    FieldClassificationModelEncoder&&) = default;
FieldClassificationModelEncoder::~FieldClassificationModelEncoder() = default;

std::string_view FieldClassificationModelEncoder::FindTokenById(
    TokenId id) const {
  return dictionary_.FindTokenById(id);
}

FieldClassificationModelEncoder::TokenId
FieldClassificationModelEncoder::TokenToId(std::string_view token) const {
  return dictionary_.TokenToId(token);
}

std::vector<std::vector<FieldClassificationModelEncoder::TokenId>>
FieldClassificationModelEncoder::EncodeForm(const FormData& form) const {
  size_t num_form_fields_to_encode = std::min(
      form.fields().size(),
      static_cast<size_t>(encoding_parameters_.maximum_number_of_fields()));
  // Form-level features are encoded in an additional field.
  std::vector<std::vector<TokenId>> encoded_form(
      ShouldEncodeFormLevelFeatures() ? num_form_fields_to_encode + 1
                                      : num_form_fields_to_encode);

  for (size_t i = 0; i < num_form_fields_to_encode; ++i) {
    encoded_form[i] = EncodeField(form.fields()[i]);
  }

  if (ShouldEncodeFormLevelFeatures()) {
    encoded_form[num_form_fields_to_encode] = EncodeFormFeatures(form);
  }
  return encoded_form;
}

std::vector<FieldClassificationModelEncoder::TokenId>
FieldClassificationModelEncoder::EncodeField(const FormFieldData& field) const {
  // Protobuf does not generate an `enum class`. Therefore, this points to the
  // wrapping container class.
  using FeaturesEnum =
      optimization_guide::proto::AutofillFieldClassificationEncodingParameters;

  auto encode = [&](int feature) -> std::vector<TokenId> {
    static_assert(FeaturesEnum::AutofillFieldClassificationFeature_MAX ==
                      FeaturesEnum::FEATURE_TYPE,
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
      case FeaturesEnum::FEATURE_TYPE:
        return EncodeAttribute(base::UTF8ToUTF16(
            FormControlTypeToString(field.form_control_type())));
    }
    return {};
  };
  std::vector<TokenId> output;
  output.reserve(GetFieldEncodingSize(encoding_parameters_));
  output.emplace_back(GetClsToken());

  for (int feature : encoding_parameters_.features()) {
    std::ranges::move(encode(feature), std::back_inserter(output));
  }

  // Pad the remaining space, if any, with zeroes.
  std::fill_n(std::back_inserter(output), output.capacity() - output.size(),
              FieldClassificationModelEncoderDictionary::kPaddingTokenId);

  return output;
}

std::vector<FieldClassificationModelEncoder::TokenId>
FieldClassificationModelEncoder::EncodeFormFeatures(
    const FormData& form) const {
  // Protobuf does not generate an `enum class`. Therefore, this points to the
  // wrapping container class.
  using FeaturesEnum =
      optimization_guide::proto::AutofillFieldClassificationEncodingParameters;

  auto encode = [&](int feature) -> std::vector<TokenId> {
    static_assert(
        FeaturesEnum::AutofillFieldClassificationFormLevelFeature_MAX ==
            FeaturesEnum::FEATURE_FRAME_URL_PATH,
        "Update the switch when adding more features");
    switch (feature) {
      case FeaturesEnum::FEATURE_UNKNOWN:
        return {};
      case FeaturesEnum::FEATURE_FORM_BUTTON_TITLES:
        return EncodeAttribute(GetButtonTitlesString(form.button_titles()));
      case FeaturesEnum::FEATURE_FORM_ID:
        return EncodeAttribute(form.id_attribute());
      case FeaturesEnum::FEATURE_FORM_NAME:
        return EncodeAttribute(form.name_attribute());
      case FeaturesEnum::FEATURE_FRAME_URL_PATH:
        return EncodeAttribute(base::UTF8ToUTF16(form.url().GetPath()));
    }
    return {};
  };
  std::vector<TokenId> output;
  output.reserve(GetFieldEncodingSize(encoding_parameters_));
  output.emplace_back(form_cls_token());

  for (int feature : encoding_parameters_.form_features()) {
    std::ranges::move(encode(feature), std::back_inserter(output));
  }

  // Pad the remaining space, if any, with zeroes.
  std::fill_n(std::back_inserter(output), output.capacity() - output.size(),
              FieldClassificationModelEncoderDictionary::kPaddingTokenId);
  return output;
}

std::u16string FieldClassificationModelEncoder::StandardizeString(
    std::u16string_view input) const {
  std::u16string standardized_input(input);
  if (encoding_parameters_.split_on_camel_case()) {
    std::string utf8_input = base::UTF16ToUTF8(standardized_input);

    static base::NoDestructor<re2::RE2> kCamelCaseLowerUpperSplitRe(
        "(\\p{Ll})(\\p{Lu})");
    static base::NoDestructor<re2::RE2> kCamelCaseAcronymSplitRe(
        "(\\p{Lu})(\\p{Lu}\\p{Ll})");
    re2::RE2::GlobalReplace(&utf8_input, *kCamelCaseLowerUpperSplitRe,
                            "\\1 \\2");
    re2::RE2::GlobalReplace(&utf8_input, *kCamelCaseAcronymSplitRe, "\\1 \\2");

    standardized_input = base::UTF8ToUTF16(utf8_input);
  }

  if (encoding_parameters_.lowercase()) {
    standardized_input = base::i18n::ToLower(standardized_input);
  }

  base::ReplaceChars(
      standardized_input,
      base::UTF8ToUTF16(encoding_parameters_.replace_chars_with_whitespace()),
      u" ", &standardized_input);

  base::RemoveChars(standardized_input,
                    base::UTF8ToUTF16(encoding_parameters_.remove_chars()),
                    &standardized_input);
  return standardized_input;
}

std::vector<FieldClassificationModelEncoder::TokenId>
FieldClassificationModelEncoder::EncodeAttribute(
    std::u16string_view input) const {
  std::u16string standardized_input = StandardizeString(input);

  std::vector<std::u16string_view> split_string =
      base::SplitStringPiece(standardized_input, u" ", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  // Padding the output to be of size `max_tokens_per_feature`.
  split_string.resize(encoding_parameters_.max_tokens_per_feature(), u"");

  // The UTF-16 to UTF-8 conversion is done per token, rather than once on
  // `standardized_input`, because individual tokens are short enough that the
  // small string optimization typically avoids a heap allocation. Splitting in
  // UTF-16 also trims non-ASCII whitespace (e.g. U+00A0) from the tokens, which
  // splitting the UTF-8 string wouldn't.
  return base::ToVector(split_string, [&](std::u16string_view token) {
    return token.empty()
               ? FieldClassificationModelEncoderDictionary::kPaddingTokenId
               : TokenToId(base::UTF16ToUTF8(token));
  });
}

bool FieldClassificationModelEncoder::ShouldEncodeFormLevelFeatures() const {
  return encoding_parameters_.form_features().size() > 0;
}

FieldClassificationModelEncoder::TokenId
FieldClassificationModelEncoder::form_cls_token() const {
  CHECK(ShouldEncodeFormLevelFeatures());
  return TokenId(dictionary_.GetVocabularySize() + 1);
}

}  // namespace autofill
