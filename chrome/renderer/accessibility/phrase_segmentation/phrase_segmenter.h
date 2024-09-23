// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_PHRASE_SEGMENTER_H_
#define CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_PHRASE_SEGMENTER_H_

#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/base_phrase_segmenter.h"
#include "chrome/renderer/accessibility/phrase_segmentation/token_boundaries.h"
#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"

// PhraseSegmenter breaks an input text into segments (phrases) by
// breaking the dependency graph of the tokens of the text.
// The algorithm is described in go/smart-highlighting.
class PhraseSegmenter : public BasePhraseSegmenter {
 public:
  // Returns a score for every boundary indicating the likelihood of breaking.
  // The inputs are the tokenized sentence, the calculated token boundaries, the
  // strategy for finding the segments ('words' or 'characters') and the
  // threshold for the strategy.
  std::vector<float> CalculatePhraseBoundariesVector(
      const TokenizedSentence& tokenized_sentence,
      const TokenBoundaries& token_boundaries,
      Strategy strategy,
      int threshold) override;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_PHRASE_SEGMENTER_H_
