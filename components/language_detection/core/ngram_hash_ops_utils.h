// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_NGRAM_HASH_OPS_UTILS_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_NGRAM_HASH_OPS_UTILS_H_

#include <string>
#include <utility>
#include <vector>

namespace language_detection {

class TokenizedOutput {
 public:
  TokenizedOutput();
  ~TokenizedOutput();
  TokenizedOutput(const TokenizedOutput& rhs);

  // The processed string (with necessary prefix, suffix, skipped tokens, etc.).
  std::string str;
  // This vector contains pairs, where each pair has two members. The first
  // denoting the starting index of the token in the `str` string, and the
  // second denoting the length of that token in bytes.
  std::vector<std::pair<size_t, size_t>> tokens;
};

// Tokenizes the given input string on Unicode token boundaries, with a maximum
// of `max_tokens` tokens.
//
// If `exclude_nonalphaspace_tokens` is enabled, the tokenization ignores
// non-alphanumeric tokens, and replaces them with a replacement token (" ").
//
// The method returns the output in the `TokenizedOutput` struct, which stores
// both, the processed input string, and the indices and sizes of each token
// within that string.
TokenizedOutput Tokenize(const char* input_str,
                         size_t len,
                         size_t max_tokens,
                         bool exclude_nonalphaspace_tokens);

// Converts the given unicode string (`input_str`) with the specified length
// (`len`) to a lowercase string.
//
// The method populates the lowercased string in `output_str`.
void LowercaseUnicodeStr(const char* input_str,
                         int len,
                         std::string* output_str);

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_NGRAM_HASH_OPS_UTILS_H_
