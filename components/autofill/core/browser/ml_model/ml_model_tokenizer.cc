// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/ml_model_tokenizer.h"

#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {

namespace {

constexpr AutofillMLModelTokenizer::TokenId kUnknownToken =
    AutofillMLModelTokenizer::TokenId(1);

}  // namespace

AutofillMLModelTokenizer::AutofillMLModelTokenizer(
    std::vector<std::pair<std::u16string, TokenId>> entries)
    : token_to_id_(
          base::MakeFlatMap<std::u16string, TokenId>(std::move(entries))) {}

AutofillMLModelTokenizer::AutofillMLModelTokenizer(
    const AutofillMLModelTokenizer& tokenizer) = default;
AutofillMLModelTokenizer::~AutofillMLModelTokenizer() = default;

// static
std::unique_ptr<AutofillMLModelTokenizer>
AutofillMLModelTokenizer::CreateTokenizer(
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
  // if the output_sequence_length = 5 then the output should be [8,2,0,0,0].
  CHECK(tokens[0].empty());
  // Index 1 is reserved for words not in the dictionary. For example,
  // if the word "last" is not in the dictionary, then its token index is 1.
  CHECK(tokens[1] == "[UNK]");
  for (const std::string& token : tokens) {
    AutofillMLModelTokenizer::TokenId id(entries.size());
    entries.emplace_back(base::UTF8ToUTF16(token), id);
  }
  return base::WrapUnique(new AutofillMLModelTokenizer(std::move(entries)));
}

size_t AutofillMLModelTokenizer::GetDictionarySize() const {
  return token_to_id_.size();
}

AutofillMLModelTokenizer::TokenId AutofillMLModelTokenizer::TokenToId(
    const std::u16string& token) const {
  auto match = token_to_id_.find(token);
  if (match == token_to_id_.end()) {
    return kUnknownToken;
  }
  return match->second;
}

}  // namespace autofill
