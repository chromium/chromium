// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/dependency_tree.h"

#include <cstdlib>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

DependencyTree::DependencyTree(const TokenizedSentence& sentence,
                               const std::vector<int>& dependency_heads)
    : dep_head_array_(dependency_heads.size()) {
  for (int i = 0; i < static_cast<int>(dependency_heads.size()); i++) {
    dep_head_array_[i] = Dependency(sentence.tokens()[i], i,
                                    static_cast<int>(dependency_heads[i]),
                                    sentence.token_boundaries()[i].first);
  }
  CalculateSubtreeBoundaries();
}

DependencyTree::~DependencyTree() = default;

void DependencyTree::CalculateSubtreeBoundaries() {
  // Note: this is worst-case O(N^2), when the dependency tree is a linked
  // list. It's possible to implement this in O(N) with DFS/BFS if we keep
  // track of a node's children in Dependency, instead of just the parent
  // pointer. Given the small N, and the unlikeliness of worst-case, the
  // simplicity and lower space complexity of this algorithm won out.
  for (auto& token : dep_head_array_) {
    // Initialize each node's subtree boundaries to be just the node index.
    token.subtree_start = token.absolute_index;
    token.subtree_end = token.absolute_index;
  }

  bool updated = true;
  while (updated) {
    updated = false;
    for (auto token : dep_head_array_) {
      // Expand parent boundary to include all child boundaries, and repeat
      // until all boundaries have been updated.
      if (token.dependency_head != token.absolute_index) {
        Dependency* parent = &dep_head_array_[token.dependency_head];
        if (token.subtree_start < parent->subtree_start) {
          parent->subtree_start = token.subtree_start;
          updated = true;
        }
        if (token.subtree_end > parent->subtree_end) {
          parent->subtree_end = token.subtree_end;
          updated = true;
        }
      }
    }
  }
}

int DependencyTree::FindBoundaryForParentEdge(int node_index) const {
  const Dependency& child = dep_head_array_[node_index];
  const Dependency& parent = dep_head_array_[child.dependency_head];
  std::vector<int> candidates = {};
  if (child.subtree_start > parent.subtree_start) {
    candidates.push_back(child.subtree_start - 1);
  }
  if (child.subtree_end < parent.subtree_end) {
    candidates.push_back(child.subtree_end);
  }
  if (candidates.empty()) {
    return -1;
  } else if (candidates.size() == 1) {
    return candidates[0];
  } else if (parent.absolute_index < child.absolute_index) {
    // When both the left and right edges of the subtree are candidate
    // boundaries corresponding to this edge, the correct boundary is determined
    // by whether the parent is before or after the child in the sentence.
    return candidates[0];
  } else {
    return candidates[1];
  }
}
