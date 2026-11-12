// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/token_boundaries.h"

#include <algorithm>
#include <array>
#include <cstdlib>

#include "chrome/renderer/accessibility/phrase_segmentation/dependency_tree.h"

namespace {

// Constants used in the boundary weight calculation. These were calculated by
// trial and error experiments on the eval_160 dataset.

// How much to bump up the weight of a boundary with a phrase start indicator.
const float kPhraseStartWeightBump = 1.0;

// How much to bump up the weight of boundary with a phrase end indicator.
const float kPhraseEndWeightBump = 4.0;

// Punctuation that tend to mark the start of a phrase.
constexpr auto kPhraseStartIndicators = std::to_array<char16_t>({u'(', u'"'});

// Punctuation that tend to mark the end of a phrase.
constexpr auto kPhraseEndIndicators =
    std::to_array<char16_t>({u',', u';', u':', u')', u'"', u'—'});

// What weight to assign to a boundary where the two adjacent tokens have the
// same head.
const float kSameDependencyHeadWeight = 1.0;

}  // namespace

TokenBoundaries::TokenBoundaries(const DependencyTree& tree)
    : boundary_weights_(tree.dep_head_array().size() - 1, -1) {
  InitializeBoundaryWeightsFromTree(tree);
}

TokenBoundaries::~TokenBoundaries() = default;

// The weights are calculated in the following way:
// 1. By default, inter-token boundary weights are initialized from the
// corresponding edge weight in the dependency graph.
// 2. If the two adjacent tokens don't have a space between them, the weight is
// set to 0. We should only break phrases on whitespace; so for example,
// breaking *before* the comma in "a smooth, semi-solid foam" would not be
// allowed.
// 3. If, on the other hand, the first token is a "phrase end indicator" (such
// as a comma), we bump up its weight by `kPhraseEndWeightBump` to prefer
// breaking there. So breaking *after* a comma would be encouraged. Likewise if
// the second token is a phrase start indicator, such as an open bracket, we
// bump its weight by `kPhraseStartWeightBump`.
// 4. If the two adjacent tokens have the same head, the weight is set to 1.
// This is done because some tokens have many children, which causes individual
// tokens to become stranded.
void TokenBoundaries::InitializeBoundaryWeightsFromTree(
    const DependencyTree& tree) {
  for (const auto& token : tree.dep_head_array()) {
    int weight = abs(token.dependency_head - token.absolute_index);
    if (weight == 0) {
      continue;
    }

    // Start off with the boundary weight being the edge weight in the dep graph
    int boundary_index = tree.FindBoundaryForParentEdge(token.absolute_index);
    if (boundary_index == -1) {
      // This only happens either for the head of the tree, or in pathological
      // cases where the child subtree is not entirely contained within the
      // parent subtree.
      continue;
    }
    boundary_weights_[boundary_index] = weight;

    const Dependency& token1 = tree.dep_head_array()[boundary_index];
    const Dependency& token2 = tree.dep_head_array()[boundary_index + 1];
    if (token1.dependency_head == token2.dependency_head) {
      // If the adjacent tokens share the same head, bias against breaking there
      // by setting weight to kSameDependencyHeadWeight.
      boundary_weights_[boundary_index] = kSameDependencyHeadWeight;
    }

    if (token2.char_offset ==
        token1.char_offset + static_cast<int>(token1.text.length())) {
      // If the adjacent tokens aren't separated by a space, we should not
      // break there, so zero out the weight.
      boundary_weights_[boundary_index] = 0;
    } else if (token1.text.length() == 1 &&
               std::ranges::contains(kPhraseEndIndicators, token1.text[0])) {
      // If the first token is a phrase end-indicating punctuation, bias
      // towards breaking at that boundary by incrementing the weight.
      boundary_weights_[boundary_index] += kPhraseEndWeightBump;
    } else if (token2.text.length() == 1 &&
               std::ranges::contains(kPhraseStartIndicators, token2.text[0])) {
      // If the second token is a phrase start-indicating punctuation, bias
      // towards breaking at that boundary by incrementing the weight.
      boundary_weights_[boundary_index] += kPhraseStartWeightBump;
    }
  }
}
