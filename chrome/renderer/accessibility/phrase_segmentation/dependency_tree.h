// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_TREE_H_
#define CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_TREE_H_

#include <string>
#include <string_view>
#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"

// Represents a single dependency relationship between a token and its head.
// Since every token has a unique head, there is one Dependency per (child)
// token.
struct Dependency {
  // The text of the (child) token.
  std::u16string_view text;
  // The absolute index of the token in the sentence.
  int absolute_index;
  // The absolute index of the token's head in the sentence.
  int dependency_head;
  // The character offset at which this token starts in the sentence.
  int char_offset;
  // This token has a subtree in the overall dependency tree of the sentence.
  // `subtree_start` and `subtree_end` are the min and max absolute indices of
  // tokens within the subtree of this token.
  int subtree_start;
  int subtree_end;

  Dependency(std::u16string_view text,
             int absolute_index,
             int dependency_head,
             int char_offset)
      : text(text),
        absolute_index(absolute_index),
        dependency_head(dependency_head),
        char_offset(char_offset),
        subtree_start(-1),
        subtree_end(-1) {}
  Dependency() = default;
};

// Collection representing a dependency tree for a tokenized sentence. Note that
// the tree is in parent-pointer form, where each node has a link to its parent,
// but not to its children.
class DependencyTree {
 public:
  using DependencyRelationArray = std::vector<Dependency>;

  // Create a dependency tree from a tokenized sentence and an array containing
  // the index of the dependency head for each token. (The size of the
  // dependency_heads vector should be identical to the number of tokens in the
  // TokenizedSentence, and in the same order.)
  DependencyTree(const TokenizedSentence& sentence,
                 const std::vector<int>& dependency_heads);

  ~DependencyTree();

  const DependencyRelationArray& dep_head_array() const {
    return dep_head_array_;
  }

  // Determine the word boundary that corresponds to an edge in the dependency
  // graph.
  // Once we have the edge-weighted dependency graph, we need to map each
  // edge back to the word boundaries as boundary weights. For each edge in the
  // dependency graph, if we hypothetically removed that edge, the graph would
  // then split into two trees, each corresponding to a phrase. This function
  // determines the location of the phrase break corresponding to the edge.
  // Returns -1 for the tree root or for pathological cases, which should be
  // handled gracefully by the caller.
  int FindBoundaryForParentEdge(int node_index) const;

 private:
  DependencyRelationArray dep_head_array_;

  // Post-processes the dependency relation array to populate subtree start and
  // end for each node. This can only be done after all the relations are first
  // initialized.
  void CalculateSubtreeBoundaries();
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_TREE_H_
