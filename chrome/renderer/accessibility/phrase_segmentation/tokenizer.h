// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_TOKENIZER_H_
#define CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_TOKENIZER_H_

#include <string>
#include <utility>
#include <vector>

// Tokenizer class tokenizes the input text.
class Tokenizer {
 public:
  // Tokenizes the input string, and returns a list of start and end incides for
  // each token.
  std::vector<std::pair<int, int>> Tokenize(const std::u16string& input);
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_TOKENIZER_H_
