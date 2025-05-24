// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_TOKENIZED_SENTENCE_H_
#define CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_TOKENIZED_SENTENCE_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/tokenizer.h"

// Collection representing the output of the tokenization process, for use in
// downstream processing for phrase detection.
class TokenizedSentence {
 public:
  // Constructs a tokenized sentence from a string by running a tokenizer.
  explicit TokenizedSentence(const std::u16string& text);

  // Constructs a tokenized sentence from tokens.
  explicit TokenizedSentence(const std::u16string& text,
                             const std::vector<std::u16string>& tokens);

  ~TokenizedSentence();

  const std::vector<std::u16string_view>& tokens() const { return tokens_; }
  const std::vector<std::pair<int, int>>& token_boundaries() const {
    return token_boundaries_;
  }

  // Calculates the number of words between two token indices (both included).
  // This is different from simply (end_token-start_token+1), because special
  // characters such as punctuations are tokenized, but may not contribute to
  // the word count. For example, the string `(below 2 °C or 35 °F).` has 6
  // words but 11 tokens.
  int WordsBetween(unsigned int start_token, unsigned int end_token) const;

  // Calculate the number of characters between two tokens, both included. This
  // is trivially implemented, unlike WordsBetween.
  int CharactersBetween(int start, int end) const {
    return token_boundaries_[end].second - token_boundaries_[start].first;
  }

 private:
  std::vector<std::u16string_view> tokens_;
  std::vector<std::pair<int, int>> token_boundaries_;

  Tokenizer tokenizer_;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_TOKENIZED_SENTENCE_H_
