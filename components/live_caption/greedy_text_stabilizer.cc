// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/greedy_text_stabilizer.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "base/strings/string_tokenizer.h"

namespace {
std::string RemoveTrailingSpace(const std::string& input) {
  if (input.length() > 0 && std::isspace(input.back())) {
    return input.substr(0, input.length() - 1);
  } else {
    return input;
  }
}
}  // namespace

namespace captions {

GreedyTextStabilizer::GreedyTextStabilizer(int min_token_frequency)
    : min_token_frequency_(min_token_frequency < 0 ? 0 : min_token_frequency) {}

GreedyTextStabilizer::~GreedyTextStabilizer() = default;

std::string GreedyTextStabilizer::UpdateText(const std::string& input_text,
                                             const bool is_final) {
  // For final recognition results, we use all tokens even if they are unstable.
  // Reset the stabilizer in preparation for receiving new partial recognition
  // results.
  if (is_final) {
    Reset();
    return input_text;
  }

  const std::vector<std::string> tokens = Tokenize(input_text);

  // When min_token_frequency_ is 0, we define the output to be the input.
  // Therefore, we can exit early.
  if (min_token_frequency_ == 0) {
    stable_text_ = input_text;
    stable_token_count_ = tokens.size();
    return input_text;
  }

  // Add each token to the correct position in the tokens dictionary.
  for (unsigned long i = 0; i < tokens.size(); ++i) {
    const std::string token = RemoveTrailingSpace(tokens[i]);
    // If this location in the sentence does not yet exist, we need to extend
    // the vector to include this location.
    if (i >= tokens_histograms_.size()) {
      std::unordered_map<std::string, int> token_histogram = {{token, 1}};
      tokens_histograms_.push_back(token_histogram);
    } else {
      // Increment the count of the token in the dictionary at this location.
      tokens_histograms_[i][token]++;
    }
  }

  // Now compare the input tokens to those in the distributions.
  // As we consider the token at each location, we determine it to be stable if
  // it is the mode in the token dictionary for that location and its token
  // frequency is high enough. Otherwise, it is considered unstable, and we exit
  // early.
  stable_token_count_ = 0;
  int stable_character_count = 0;
  for (unsigned long i = 0; i < tokens.size(); i++) {
    const std::string token = RemoveTrailingSpace(tokens[i]);
    if (i < tokens_histograms_.size() &&
        tokens_histograms_[i][token] >= min_token_frequency_ &&
        IsMode(token, tokens_histograms_[i])) {
      // Use the size of the unstripped token.
      stable_character_count += tokens[i].size();
      stable_token_count_++;
    } else {
      break;
    }
  }

  // Only use the new text if it has more tokens than the previous stable text.
  // This prevents shrinkage of the text.
  if (stable_token_count_ >= max_stable_token_count_) {
    max_stable_token_count_ = stable_token_count_;

    // Update the stable text.
    if (stable_token_count_ <= 0) {
      stable_text_ = std::string();
    } else {
      stable_text_ = input_text.substr(0, stable_character_count);
    }
  }

  return stable_text_;
}

void GreedyTextStabilizer::Reset() {
  max_stable_token_count_ = 0;
  stable_token_count_ = 0;
  stable_text_ = std::string();
  tokens_histograms_.clear();
}

std::vector<std::string> GreedyTextStabilizer::Tokenize(
    const std::string& input_text) {
  std::vector<std::string> tokens;

  base::StringTokenizer t(input_text, " ");
  t.set_options(base::StringTokenizer::RETURN_DELIMS);
  while (t.GetNext()) {
    // Trailing punctuation should be treated as a separate token so that
    // flickering punctuation can be handled appropriately.
    if (t.token().size() > 0 && std::ispunct(t.token().back())) {
      tokens.push_back(t.token().substr(0, t.token().size() - 1));
      tokens.push_back(t.token().substr(t.token().size() - 1, 1));
    } else {
      tokens.push_back(t.token());
    }
  }

  return tokens;
}

bool GreedyTextStabilizer::IsMode(
    const std::string& token,
    const std::unordered_map<std::string, int>& token_histogram) {
  const int token_count = token_histogram.at(token);

  // There could be multiple modes in the histogram, and we only need to ensure
  // that the given token is one of the modes.  Thus, the given token is a mode
  // only if no other token has a higher count than the given token.
  for (const auto& element : token_histogram) {
    if (element.second > token_count) {
      // If we have found a token with a higher count, we exit early and
      // indicate that the given token is not a mode.
      return false;
    }
  }
  return true;
}

}  // namespace captions
