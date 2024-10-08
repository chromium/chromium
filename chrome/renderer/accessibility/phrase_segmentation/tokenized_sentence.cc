// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"

#include "tokenized_sentence.h"

TokenizedSentence::TokenizedSentence(const std::u16string& text) {
  token_boundaries_ = tokenizer_.Tokenize(text);
  tokens_.reserve(token_boundaries_.size());
  for (const std::pair<int, int>& boundary : token_boundaries_) {
    tokens_.emplace_back(std::u16string_view(text).substr(
        boundary.first, boundary.second - boundary.first));
  }
}

// Constructs a tokenized sentence from tokens.
TokenizedSentence::TokenizedSentence(
    const std::u16string& text,
    const std::vector<std::u16string>& tokens) {
  token_boundaries_.reserve(tokens.size());
  tokens_.reserve(tokens.size());
  for (unsigned int i = 0, start = 0; i < tokens.size(); i++) {
    int begin = text.find(tokens[i], start);
    int end = begin + tokens[i].size();
    token_boundaries_.emplace_back(begin, end);
    tokens_.emplace_back(std::u16string_view(text).substr(begin, end - begin));
    start = end;
  }
}

TokenizedSentence::~TokenizedSentence() = default;

int TokenizedSentence::WordsBetween(unsigned int start_token,
                                    unsigned int end_token) const {
  if (end_token > token_boundaries_.size()) {
    end_token = token_boundaries_.size() - 1;
  }
  if (start_token > end_token) {
    return 0;
  }

  int words_between = 1;

  for (unsigned int i = start_token; i < end_token; ++i) {
    if (token_boundaries_[i].second < token_boundaries_[i + 1].first) {
      // If there are spaces after a token, that shows up as end_offset being
      // less than next_start_offset (to accommodate the space), and thus
      // signifies a word.
      ++words_between;
    }
  }
  return words_between;
}
