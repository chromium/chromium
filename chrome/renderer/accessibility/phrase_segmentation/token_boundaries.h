// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_TOKEN_BOUNDARIES_H_
#define CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_TOKEN_BOUNDARIES_H_

#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/dependency_tree.h"

// Container for inter-token boundary weights. Phrase boundaries are basically
// the boundaries between words / tokens where the sentence must be broken to
// yield phrases. So each boundary between tokens is a candidate for a phrase
// boundary also. We assign a weight for each boundary between tokens, which
// are used in the phrase breaking algorithm. This class holds the boundary
// weights for a particular sentence, initialized from its dependency tree.
class TokenBoundaries {
 public:
  explicit TokenBoundaries(const DependencyTree& tree);
  ~TokenBoundaries();

  const std::vector<float>& boundary_weights() const {
    return boundary_weights_;
  }

 private:
  // Calculate the weight of each inter-token boundary in the sentence. These
  // weights are then used for the phrase breaking algorithm.
  void InitializeBoundaryWeightsFromTree(const DependencyTree& tree);

  std::vector<float> boundary_weights_;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_TOKEN_BOUNDARIES_H_
