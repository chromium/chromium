// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/language_detection/core/ngram_hash_ops_utils.h"

#include <cstring>

#include "third_party/utf/src/include/utf.h"

namespace language_detection {

constexpr char kPrefix[] = "^";
constexpr char kSuffix[] = "$";
constexpr char kReplacementToken[] = " ";

TokenizedOutput::TokenizedOutput() = default;

TokenizedOutput::~TokenizedOutput() = default;

TokenizedOutput::TokenizedOutput(const TokenizedOutput& rhs) = default;

TokenizedOutput Tokenize(const char* input_str,
                         size_t len,
                         size_t max_tokens,
                         bool exclude_nonalphaspace_tokens) {
  TokenizedOutput output;
  size_t token_start = 0;
  output.str.reserve(len + 2);
  output.tokens.reserve(len + 2);
  output.str.append(kPrefix);
  output.tokens.emplace_back(std::make_pair(token_start, strlen(kPrefix)));
  token_start += strlen(kPrefix);
  Rune token;
  for (size_t i = 0; i < len && output.tokens.size() + 1 < max_tokens;) {
    if (input_str == nullptr)
      break;
    if (strlen(input_str) == 0)
      break;

    // Use the standard UTF-8 library to find the next token.
    size_t bytes_read = charntorune(&token, input_str + i, len - i);
    // Stop processing, if we can't read any more tokens, or we have reached
    // maximum allowed tokens, allocating one token for the suffix.
    if (bytes_read == 0) {
      break;
    }
    // If `exclude_nonalphaspace_tokens` is set to true, and the token is not
    // alphanumeric, replace it with a replacement token.
    if (exclude_nonalphaspace_tokens && !isalpharune(token)) {
      output.str.append(kReplacementToken);
      output.tokens.emplace_back(
          std::make_pair(token_start, strlen(kReplacementToken)));
      token_start += strlen(kReplacementToken);
      i += bytes_read;
      continue;
    }
    // Append the token in the output string, and note its position and the
    // number of bytes that token consumed.
    output.str.append(input_str + i, bytes_read);
    output.tokens.emplace_back(std::make_pair(token_start, bytes_read));
    token_start += bytes_read;
    i += bytes_read;
  }
  output.str.append(kSuffix);
  output.tokens.emplace_back(std::make_pair(token_start, strlen(kSuffix)));
  token_start += strlen(kSuffix);
  return output;
}

void LowercaseUnicodeStr(const char* input_str,
                         int len,
                         std::string* output_str) {
  for (int i = 0; i < len;) {
    Rune token;
    // Tokenize the given string, and get the appropriate lowercase token.
    size_t bytes_read = charntorune(&token, input_str + i, len - i);
    token = isalpharune(token) ? tolowerrune(token) : token;
    // Write back the token to the output string.
    char token_buf[UTFmax];
    size_t bytes_to_write = runetochar(token_buf, &token);
    output_str->append(token_buf, bytes_to_write);
    i += bytes_read;
  }
}

}  // namespace language_detection
