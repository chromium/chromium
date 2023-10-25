// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace autofill {

namespace {

constexpr AutofillModelVectorizer::TokenId kUnknownTokenId =
    AutofillModelVectorizer::TokenId(1);

}  // namespace

AutofillModelVectorizer::AutofillModelVectorizer(
    const google::protobuf::RepeatedPtrField<std::string>& tokens) {
  std::vector<std::pair<std::u16string, AutofillModelVectorizer::TokenId>>
      entries = {
          // Index 0 is reserved for padding to `kOutputSequenceLength`.
          // For example, a label "first name" is encoded as [?, ?, 0] if the
          // output sequence length is 3.
          {u"", AutofillModelVectorizer::TokenId(0)},
          // Index 1 is reserved for words not in the dictionary.
          {u"", kUnknownTokenId},
      };
  entries.reserve(2 + tokens.size());
  size_t i = 2;
  for (const std::string& token : tokens) {
    entries.emplace_back(base::UTF8ToUTF16(token),
                         AutofillModelVectorizer::TokenId(i++));
  }
  token_to_id_ = base::flat_map<std::u16string, TokenId>(std::move(entries));
}

AutofillModelVectorizer::AutofillModelVectorizer() = default;
AutofillModelVectorizer::AutofillModelVectorizer(
    const AutofillModelVectorizer&) = default;
AutofillModelVectorizer::~AutofillModelVectorizer() = default;

AutofillModelVectorizer::TokenId AutofillModelVectorizer::TokenToId(
    std::u16string_view token) const {
  auto match = token_to_id_.find(token);
  if (match == token_to_id_.end()) {
    return kUnknownTokenId;
  }
  return match->second;
}

std::array<AutofillModelVectorizer::TokenId,
           AutofillModelVectorizer::kOutputSequenceLength>
AutofillModelVectorizer::Vectorize(std::u16string_view input) const {
  std::u16string standardized_input = base::ToLowerASCII(input);
  base::RemoveChars(standardized_input, kSpecialChars, &standardized_input);
  std::vector<std::u16string> split_string =
      base::SplitString(standardized_input, u" ", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  // Padding the output to be of size `kOutputSequenceLength`.
  split_string.resize(kOutputSequenceLength, u"");
  std::array<TokenId, kOutputSequenceLength> output;
  base::ranges::transform(
      split_string, output.begin(),
      [&](std::u16string_view token) { return TokenToId(token); });
  return output;
}

}  // namespace autofill
