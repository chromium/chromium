// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_GREEDY_TEXT_STABILIZER_H_
#define COMPONENTS_LIVE_CAPTION_GREEDY_TEXT_STABILIZER_H_

#include <string>
#include <unordered_map>
#include <vector>

namespace captions {

// Predicts the stable count of streaming token sequences.
// This class has no knowledge of future partial results.  It simply
// receives one partial at a time and is forced to make a prediction on
// the number of stable tokens using only the past partial results.
// Implementation based off of https://goto.google.com/w2t.
class GreedyTextStabilizer {
 public:
  // Initializes min_token_frequency and sets it to 0 if it is non-negative.
  explicit GreedyTextStabilizer(int min_token_frequency);
  ~GreedyTextStabilizer();

  // Updates the internal structure that determines the number of stable tokens
  // and returns the stable text.
  std::string UpdateText(const std::string& input_text, bool is_final = false);

  // Used by unit test only.
  int GetStableTokenCount() const { return stable_token_count_; }

 private:
  // Minimum number of times a token must appear to be counted.
  const int min_token_frequency_ = 0;

  // List of token counts at different locations in the sequence.
  std::vector<std::unordered_map<std::string, int>> tokens_histograms_;
  std::string stable_text_ = "";
  int max_stable_token_count_ = 0;
  int stable_token_count_ = 0;

  void Reset();

  // Divides a string of text into individual tokens independent of language.
  std::vector<std::string> Tokenize(const std::string& input_text);

  // Checks whether the input token is a mode of the token histogram.
  bool IsMode(const std::string& token,
              const std::unordered_map<std::string, int>& token_histogram);
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_GREEDY_TEXT_STABILIZER_H_
