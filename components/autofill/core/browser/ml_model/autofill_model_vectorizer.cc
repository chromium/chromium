// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {

namespace {

constexpr AutofillModelVectorizer::TokenId kUnknownTokenId =
    AutofillModelVectorizer::TokenId(1);

}  // namespace

AutofillModelVectorizer::AutofillModelVectorizer(
    std::vector<std::pair<std::u16string, TokenId>> entries)
    : token_to_id_(
          base::MakeFlatMap<std::u16string, TokenId>(std::move(entries))) {}

AutofillModelVectorizer::AutofillModelVectorizer(
    const AutofillModelVectorizer& vectorizer) = default;
AutofillModelVectorizer::~AutofillModelVectorizer() = default;

// static
std::unique_ptr<AutofillModelVectorizer>
AutofillModelVectorizer::CreateVectorizer(
    const base::FilePath& dictionary_filepath) {
  std::string dictionary_content;
  if (!base::ReadFileToString(dictionary_filepath, &dictionary_content)) {
    return nullptr;
  }
  std::vector<std::string> tokens = base::SplitString(
      dictionary_content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<std::pair<std::u16string, TokenId>> entries;
  // First 2 indices are reserved.
  CHECK(tokens.size() > 1);
  // Index 0 is reserved for padding. For example, if the field
  // label "first name" and the token index for "first" = 8, "name" = 2 and
  // if the `kOutputSequenceLength` = 5 then the output should be [8,2,0,0,0].
  CHECK(tokens[0].empty());
  // Index 1 is reserved for words not in the dictionary. For example,
  // if the word "last" is not in the dictionary, then its token index is 1.
  CHECK(tokens[1] == "[UNK]");
  for (const std::string& token : tokens) {
    AutofillModelVectorizer::TokenId id(entries.size());
    entries.emplace_back(base::UTF8ToUTF16(token), id);
  }
  return base::WrapUnique(new AutofillModelVectorizer(std::move(entries)));
}

size_t AutofillModelVectorizer::GetDictionarySize() const {
  return token_to_id_.size();
}

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
