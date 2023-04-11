// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_MODEL_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

// On device head suggest feature uses an on device model which encodes some
// top queries into a radix tree (https://en.wikipedia.org/wiki/Radix_tree), to
// help users quickly get head suggestions when they are under poor network
// condition. When serving, it performs a search in the tree similar as BFS but
// only keeping children with high scores, to find top N queries which match
// the given prefix.
//
// Each node in the tree is encoded using following format to optimize storage
// (see on_device_head_model_unittest.cc for an example tree model):
// ------------------------------------------------------------------------
// | max_score_as_root | child_0 | child_1 | ... | child_n-1 | 0 (1 byte) |
// ------------------------------------------------------------------------
//
// Usage of each block in the node:
// 1) Block max_score_as_root at the beginning of each node contains the
// maximum leaf score can be found in its subtree, which is used for pruning
// during tree traversal to improve the search performance: for example,
// imagining we have already visited some nodes, sorted them based on their
// scores and saved some of them in a structure; now we meet a node with higher
// max_score_as_root, since we know we should only show users top N suggestions
// with highest scores, we can quickly determine whether we can discard some
// node with lower max_score_as_root without physically visiting any of its
// children, as none of the children has a score higher than this low
// max_score_as_root.
// This block has following format:
//    --------------------------------------
//    | 1 (1 bit) | score_max | leaf_score |
//    --------------------------------------
//                    OR
//    -------------------------
//    | 0 (1 bit) | score_max |
///   -------------------------
//   1-bit indicator: whether there is a leaf_score at the end of this block.
//   score_max: the maximum leaf_score can be found if using current node as
//      root.
//   leaf_score: only exists when indicator is 1; it is the score of some
//      complete suggestion ends at current node.
//
// 2) Block child_i (0 <= i <= n-1) has following format:
//    -------------------------------------------------------------
//    | length of text (1 byte) | text | 1 | address of next node |
//    -------------------------------------------------------------
//                                OR
//    ---------------------------------------------------
//    | length of text (1 byte) | text | 0 | leaf_score |
//    ---------------------------------------------------
// We use 1 bit after text field as an indicator to determine whether this child
// is an intermediate node or leaf node. If it is a leaf node, the sequence of
// texts visited so far from the start node to here can be returned as a valid
// suggestion to users with leaf_score.
//
// The size of score and address will be given in the first two bytes of the
// model file.
class OnDeviceHeadModel {
 public:
  // Gets top "max_num_matches_to_return" suggestions and their scores which
  // matches given prefix.
  static std::vector<std::pair<std::string, uint32_t>> GetSuggestionsForPrefix(
      const std::string& model_filename,
      const uint32_t max_num_matches_to_return,
      const std::string& prefix);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_MODEL_H_
