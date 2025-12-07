// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_BASE_PHRASE_SEGMENTER_H_
#define CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_BASE_PHRASE_SEGMENTER_H_

#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/token_boundaries.h"
#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"

// Strategy determines how the phrase segmenter identifies acceptable phrases.
// The phrase segmentation calculation takes one of the strategies below, along
// with a threshold to determine phrase eligibility. For example, if the caller
// wants that phrases can be max 5 words, strategy will be kWords and threshold
// will be 5.
enum class Strategy {
  kWords,
  kCharacters,
};

// An abstract phrase segmenter that breaks an input text into segments
// (phrases) according to a specific algorithm (e.g. smart breaking, random
// breaking, etc.).
class BasePhraseSegmenter {
 public:
  virtual ~BasePhraseSegmenter() = default;
  // Returns a score for every boundary indicating the likelihood of breaking.
  // The inputs are the tokenized sentence, the calculated token boundaries, the
  // strategy for finding the segments ('words' or 'characters') and the
  // threshold for the strategy.
  virtual std::vector<float> CalculatePhraseBoundariesVector(
      const TokenizedSentence& tokenized_sentence,
      const TokenBoundaries& token_boundaries,
      Strategy strategy,
      int threshold) = 0;
};

// Returns the character offsets for splitting the text into phrases.
// The inputs are the tokenized sentence, the calculated token boundaries, the
// strategy for finding the segments ('words' or 'characters') and the
// threshold for the strategy.
std::vector<int> CalculatePhraseBoundaries(
    BasePhraseSegmenter& phrase_segmenter,
    const TokenizedSentence& tokenized_sentence,
    const TokenBoundaries& token_boundaries,
    Strategy strategy,
    int threshold);

#endif  // CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_BASE_PHRASE_SEGMENTER_H_
