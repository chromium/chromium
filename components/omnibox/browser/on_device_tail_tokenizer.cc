// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_tail_tokenizer.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/omnibox/browser/omnibox_field_trial.h"

namespace {
// Maximum vocabulary file size that will be loaded in bytes.
static constexpr size_t kVocabFileSizeLimit = 64 * 1024;

// The max num of single char tokens where token IDs are directly mapped to
// ASCII characters.
// Token IDs greater than kNumSingleChar are special control tokens or
// multi-char tokens specified by the given vocabulary file.
static constexpr size_t kNumSingleChar = 256;

// Special control tokens.
static constexpr char kBeginQueryToken[] = "<Q>";
static constexpr char kEndQueryToken[] = "</Q>";
static constexpr char kEmptyPreviousQueryToken[] = "<NPQ>";
static constexpr char kUnknownToken[] = "<UNK>";

std::ostream& operator<<(std::ostream& os,
                         const base::flat_set<std::string>& tokens) {
  if (tokens.empty()) {
    return os;
  }

  auto iter = tokens.begin();
  os << *iter;
  ++iter;

  for (; iter != tokens.end(); iter++) {
    os << ", " << *iter;
  }
  return os;
}

}  // namespace

OnDeviceTailTokenizer::Tokenization::Tokenization() = default;

OnDeviceTailTokenizer::Tokenization::~Tokenization() = default;

OnDeviceTailTokenizer::OnDeviceTailTokenizer() = default;

OnDeviceTailTokenizer::~OnDeviceTailTokenizer() = default;

bool OnDeviceTailTokenizer::Init(const base::FilePath& vocabulary_filepath) {
  std::string vocabulary_content;
  Reset();
  if (!base::ReadFileToStringWithMaxSize(
          vocabulary_filepath, &vocabulary_content, kVocabFileSizeLimit)) {
    DVLOG(1) << "Failed to read the vocabulary file "
             << vocabulary_filepath.LossyDisplayName();
    return false;
  }

  base::flat_set<std::string> control_tokens = {
      kBeginQueryToken, kEndQueryToken, kEmptyPreviousQueryToken,
      kUnknownToken};

  std::string token;
  max_token_length_ = 0;

  // The first 256 tokens are ASCII characters.
  for (size_t i = 0; i < kNumSingleChar; i++) {
    token = static_cast<char>(i);
    InsertTokenToMaps(token);
  }

  std::stringstream vocabulary(vocabulary_content);
  while (std::getline(vocabulary, token)) {
    if (token.empty()) {
      break;
    }

    // Duplicate tokens are not allowed.
    if (token_to_id_.find(token) != token_to_id_.end()) {
      Reset();
      DVLOG(1) << "Duplicate token found: " << token;
      return false;
    }

    InsertTokenToMaps(token);
    if (control_tokens.find(token) != control_tokens.end()) {
      control_tokens.erase(token);
    } else {
      max_token_length_ = std::max<size_t>(max_token_length_, token.size());
    }
  }

  // A valid vocabulary should include all control tokens.
  if (!control_tokens.empty()) {
    Reset();
    DVLOG(1) << "Missing following control tokens: " << control_tokens;
    return false;
  }

  InitAmbiguousMap();

  return IsReady();
}

bool OnDeviceTailTokenizer::IsReady() const {
  return !token_to_id_.empty();
}

void OnDeviceTailTokenizer::Reset() {
  token_to_id_.clear();
  id_to_token_.clear();
  ambiguous_tokens_.clear();
}

std::string OnDeviceTailTokenizer::IdToToken(const TokenId token_id) const {
  if (token_id < 0 || static_cast<size_t>(token_id) >= id_to_token_.size()) {
    return kUnknownToken;
  }
  return id_to_token_[token_id];
}

OnDeviceTailTokenizer::TokenId OnDeviceTailTokenizer::TokenToId(
    const std::string& token) const {
  auto match = token_to_id_.find(token);
  if (match == token_to_id_.end()) {
    // The ID for unknown token.
    return token_to_id_.find(kUnknownToken)->second;
  }
  return match->second;
}

void OnDeviceTailTokenizer::InitAmbiguousMap() {
  base::flat_map<std::string, size_t> prefix_count;
  for (const std::string& token : id_to_token_) {
    // Skip special tokens.
    if (token[0] == '<') {
      continue;
    }

    for (size_t len = 1; len <= token.size(); len++) {
      prefix_count[token.substr(0, len)] += 1;
    }
  }

  // Marks tokens as ambiguous if corresponding prefixes occur multiple times.
  for (const auto& iter : prefix_count) {
    if (iter.second > 1) {
      ambiguous_tokens_.insert(iter.first);
    }
  }
}

bool OnDeviceTailTokenizer::IsBeginQueryTokenId(TokenId token_id) const {
  return token_id == TokenToId(kBeginQueryToken);
}

bool OnDeviceTailTokenizer::IsEndQueryTokenId(TokenId token_id) const {
  return token_id == TokenToId(kEndQueryToken);
}

OnDeviceTailTokenizer::TokenId OnDeviceTailTokenizer::GetEndQueryTokenId()
    const {
  return TokenToId(kEndQueryToken);
}

bool OnDeviceTailTokenizer::IsAmbiguousToken(const std::string& token) const {
  return ambiguous_tokens_.find(token) != ambiguous_tokens_.end();
}

bool OnDeviceTailTokenizer::IsTokenPrintable(TokenId token_id) const {
  if (static_cast<size_t>(token_id) >= vocab_size()) {
    return false;
  }
  // If the token is not a single character, check whether it is a special
  // control token. Note other multi-char tokens which are extracted from
  // queries are always printable.
  if (static_cast<size_t>(token_id) >= kNumSingleChar) {
    return token_id != TokenToId(kBeginQueryToken) &&
           token_id != TokenToId(kEndQueryToken) &&
           token_id != TokenToId(kEmptyPreviousQueryToken) &&
           token_id != TokenToId(kUnknownToken);
  }
  return base::IsAsciiPrintable(static_cast<char>(token_id));
}

void OnDeviceTailTokenizer::EncodeRawString(
    const std::string& raw_string,
    std::vector<std::pair<std::string, TokenId>>* token_and_ids) const {
  size_t i = 0;
  while (i < raw_string.size()) {
    // Tries longest possible matches first and reduces the length gradually
    // until a match is found.
    size_t len = std::min<size_t>(max_token_length_, raw_string.size() - i);
    while (len >= 1) {
      auto iter = token_to_id_.find(raw_string.substr(i, len));
      if (iter != token_to_id_.end()) {
        token_and_ids->push_back({iter->first, iter->second});
        i += len;
        break;
      }
      len--;
    }

    // Unknown token is found.
    if (len == 0) {
      DVLOG(1) << "Invalid token found for raw string: " << raw_string;
      token_and_ids->clear();
      return;
    }
  }
}

void OnDeviceTailTokenizer::TokenizePrevQuery(
    const std::string& prev_query,
    TokenIds* prev_query_token_ids) const {
  prev_query_token_ids->clear();

  if (prev_query.empty()) {
    // Uses the special control token <NPQ> to mark empty previous query.
    prev_query_token_ids->push_back(TokenToId(kEmptyPreviousQueryToken));
    return;
  }

  std::vector<std::pair<std::string, TokenId>> token_and_ids;
  EncodeRawString(prev_query, &token_and_ids);

  if (OmniboxFieldTrial::ShouldEncodeLeadingSpaceForOnDeviceTailSuggest()) {
    prev_query_token_ids->push_back(TokenToId(" "));
  }

  for (const auto& pair : token_and_ids) {
    prev_query_token_ids->push_back(pair.second);
  }
}

void OnDeviceTailTokenizer::CreatePrefixTokenization(
    const std::string& prefix,
    Tokenization* tokenization) const {
  std::vector<std::pair<std::string, TokenId>> token_and_ids;

  EncodeRawString(prefix, &token_and_ids);
  if (token_and_ids.empty()) {
    return;
  }

  // Checks if the last token is ambiguous.
  size_t num_unambiguous = token_and_ids.size();
  if (IsAmbiguousToken(token_and_ids[token_and_ids.size() - 1].first)) {
    num_unambiguous--;
    tokenization->constraint_prefix =
        token_and_ids[token_and_ids.size() - 1].first;
  }

  // Always add begin query token at the front of the prefix.
  tokenization->unambiguous_ids.push_back(TokenToId(kBeginQueryToken));

  if (OmniboxFieldTrial::ShouldEncodeLeadingSpaceForOnDeviceTailSuggest()) {
    tokenization->unambiguous_ids.push_back(TokenToId(" "));
  }

  for (size_t i = 0; i < num_unambiguous; ++i) {
    tokenization->unambiguous_prefix += token_and_ids[i].first;
    tokenization->unambiguous_ids.push_back(token_and_ids[i].second);
  }
}

void OnDeviceTailTokenizer::InsertTokenToMaps(const std::string& token) {
  DCHECK(!token.empty());
  token_to_id_.insert({token, id_to_token_.size()});
  id_to_token_.push_back(std::move(token));
  DCHECK_EQ(token_to_id_.size(), id_to_token_.size());
}