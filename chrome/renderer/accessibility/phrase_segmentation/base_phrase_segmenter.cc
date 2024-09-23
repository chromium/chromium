// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/base_phrase_segmenter.h"

#include <algorithm>  // For std::sort
#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/token_boundaries.h"
#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"

std::vector<int> CalculatePhraseBoundaries(
    BasePhraseSegmenter& phrase_segmenter,
    const TokenizedSentence& tokenized_sentence,
    const TokenBoundaries& token_boundaries,
    Strategy strategy,
    int threshold) {
  std::vector<float> break_likelihood =
      phrase_segmenter.CalculatePhraseBoundariesVector(
          tokenized_sentence, token_boundaries, strategy, threshold);

  std::vector<int> phrase_breaks = {0};
  for (int i = 0; i < static_cast<int>(break_likelihood.size()); ++i) {
    if (break_likelihood[i] > 0) {
      phrase_breaks.push_back(i + 1);
    }
  }

  std::sort(phrase_breaks.begin(), phrase_breaks.end());

  std::vector<int> phrase_offsets;
  phrase_offsets.reserve(phrase_breaks.size());
  for (auto index : phrase_breaks) {
    phrase_offsets.emplace_back(
        tokenized_sentence.token_boundaries()[index].first);
  }

  return phrase_offsets;
}
