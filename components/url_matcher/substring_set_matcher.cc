// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/substring_set_matcher.h"

#include <stddef.h>

#include <algorithm>
#include <queue>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/numerics/checked_math.h"
#include "base/trace_event/memory_usage_estimator.h"

namespace url_matcher {

namespace {

// Compare StringPattern instances based on their string patterns.
bool ComparePatterns(const StringPattern* a, const StringPattern* b) {
  return a->pattern() < b->pattern();
}

std::vector<const StringPattern*> GetVectorOfPointers(
    const std::vector<StringPattern>& patterns) {
  std::vector<const StringPattern*> pattern_pointers;
  pattern_pointers.reserve(patterns.size());

  for (const StringPattern& pattern : patterns)
    pattern_pointers.push_back(&pattern);

  return pattern_pointers;
}

}  // namespace

SubstringSetMatcher::SubstringSetMatcher(
    const std::vector<StringPattern>& patterns)
    : SubstringSetMatcher(GetVectorOfPointers(patterns)) {}

SubstringSetMatcher::SubstringSetMatcher(
    std::vector<const StringPattern*> patterns) {
  // Ensure there are no duplicate IDs and all pattern strings are distinct.
#if DCHECK_IS_ON()
  {
    std::set<StringPattern::ID> ids;
    std::set<std::string> pattern_strings;
    for (const StringPattern* pattern : patterns) {
      CHECK(!base::Contains(ids, pattern->id()));
      CHECK(!base::Contains(pattern_strings, pattern->pattern()));
      ids.insert(pattern->id());
      pattern_strings.insert(pattern->pattern());
    }
  }
#endif

  // Compute the total number of tree nodes needed.
  std::sort(patterns.begin(), patterns.end(), ComparePatterns);
  tree_.reserve(GetTreeSize(patterns));
  BuildAhoCorasickTree(patterns);

  // Sanity check that no new allocations happened in the tree and our computed
  // size was correct.
  DCHECK_EQ(tree_.size(), static_cast<size_t>(GetTreeSize(patterns)));

  is_empty_ = patterns.empty() && tree_.size() == 1u;
}

SubstringSetMatcher::~SubstringSetMatcher() = default;

bool SubstringSetMatcher::Match(const std::string& text,
                                std::set<StringPattern::ID>* matches) const {
  const size_t old_number_of_matches = matches->size();

  // Handle patterns matching the empty string.
  const AhoCorasickNode* const root = &tree_[kRootID];
  AccumulateMatchesForNode(root, matches);

  const AhoCorasickNode* current_node = root;
  for (const char c : text) {
    NodeID child = current_node->GetEdge(c);

    // If the child not can't be found, progressively iterate over the longest
    // proper suffix of the string represented by the current node. In a sense
    // we are pruning prefixes from the text.
    while (child == kInvalidNodeID && current_node != root) {
      current_node = &tree_[current_node->failure()];
      child = current_node->GetEdge(c);
    }

    if (child != kInvalidNodeID) {
      // The string represented by |child| is the longest possible suffix of the
      // current position of |text| in the trie.
      current_node = &tree_[child];
      AccumulateMatchesForNode(current_node, matches);
    } else {
      // The empty string is the longest possible suffix of the current position
      // of |text| in the trie.
      DCHECK_EQ(root, current_node);
    }
  }

  return old_number_of_matches != matches->size();
}

size_t SubstringSetMatcher::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(tree_);
}

// static
constexpr SubstringSetMatcher::NodeID SubstringSetMatcher::kInvalidNodeID;
constexpr SubstringSetMatcher::NodeID SubstringSetMatcher::kRootID;

SubstringSetMatcher::NodeID SubstringSetMatcher::GetTreeSize(
    const std::vector<const StringPattern*>& patterns) const {
  DCHECK(std::is_sorted(patterns.begin(), patterns.end(), ComparePatterns));

  base::CheckedNumeric<NodeID> result = 1u;  // 1 for the root node.
  if (patterns.empty())
    return result.ValueOrDie();

  auto last = patterns.begin();
  auto current = last + 1;
  // For the first pattern, each letter is a label of an edge to a new node.
  result += (*last)->pattern().size();

  // For the subsequent patterns, only count the edges which were not counted
  // yet. For this it suffices to test against the previous pattern, because the
  // patterns are sorted.
  for (; current != patterns.end(); ++last, ++current) {
    const std::string& last_pattern = (*last)->pattern();
    const std::string& current_pattern = (*current)->pattern();
    size_t prefix_bound = std::min(last_pattern.size(), current_pattern.size());

    size_t common_prefix = 0;
    while (common_prefix < prefix_bound &&
           last_pattern[common_prefix] == current_pattern[common_prefix]) {
      ++common_prefix;
    }

    result -= common_prefix;
    result += current_pattern.size();
  }

  return result.ValueOrDie();
}

void SubstringSetMatcher::BuildAhoCorasickTree(
    const SubstringPatternVector& patterns) {
  DCHECK(tree_.empty());

  // Initialize root node of tree.
  tree_.emplace_back();

  // Build the initial trie for all the patterns.
  for (const StringPattern* pattern : patterns)
    InsertPatternIntoAhoCorasickTree(pattern);

  // Trie creation is complete and edges are finalized. Shrink to fit each edge
  // map to save on memory.
  for (AhoCorasickNode& node : tree_)
    node.ShrinkEdges();

  CreateFailureAndOutputEdges();
}

void SubstringSetMatcher::InsertPatternIntoAhoCorasickTree(
    const StringPattern* pattern) {
  const std::string& text = pattern->pattern();
  const std::string::const_iterator text_end = text.end();

  // Iterators on the tree and the text.
  AhoCorasickNode* current_node = &tree_[kRootID];
  std::string::const_iterator i = text.begin();

  // Follow existing paths for as long as possible.
  while (i != text_end) {
    NodeID child = current_node->GetEdge(*i);
    if (child == kInvalidNodeID)
      break;
    current_node = &tree_[child];
    ++i;
  }

  // Create new nodes if necessary.
  while (i != text_end) {
    tree_.emplace_back();
    current_node->SetEdge(*i, tree_.size() - 1);
    current_node = &tree_.back();
    ++i;
  }

  // Register match.
  current_node->SetMatchID(pattern->id());
}

void SubstringSetMatcher::CreateFailureAndOutputEdges() {
  base::queue<AhoCorasickNode*> queue;

  // Initialize the failure edges for |root| and its children.
  AhoCorasickNode* const root = &tree_[0];

  // Assigning |root| as the failure edge for itself doesn't strictly abide by
  // the definition of "proper" suffix. The proper suffix of an empty string
  // should probably be defined as null, but we assign it to the |root| to
  // simplify the code and have the invariant that the failure edge is always
  // defined.
  root->SetFailure(kRootID);

  root->SetOutputLink(kInvalidNodeID);

  NodeID root_output_link = root->IsEndOfPattern() ? kRootID : kInvalidNodeID;

  for (const auto& edge : root->edges()) {
    AhoCorasickNode* child = &tree_[edge.second];
    child->SetFailure(kRootID);
    child->SetOutputLink(root_output_link);
    queue.push(child);
  }

  // Do a breadth first search over the trie to create failure edges. We
  // maintain the invariant that any node in |queue| has had its |failure_| and
  // |output_link_| edge already initialized.
  while (!queue.empty()) {
    AhoCorasickNode* current_node = queue.front();
    queue.pop();

    // Compute the failure and output edges of children using the failure edges
    // of the current node.
    for (const auto& edge : current_node->edges()) {
      const char edge_label = edge.first;
      AhoCorasickNode* child = &tree_[edge.second];

      const AhoCorasickNode* failure_candidate_parent =
          &tree_[current_node->failure()];
      NodeID failure_candidate_id =
          failure_candidate_parent->GetEdge(edge_label);
      while (failure_candidate_id == kInvalidNodeID &&
             failure_candidate_parent != root) {
        failure_candidate_parent = &tree_[failure_candidate_parent->failure()];
        failure_candidate_id = failure_candidate_parent->GetEdge(edge_label);
      }

      if (failure_candidate_id == kInvalidNodeID) {
        DCHECK_EQ(root, failure_candidate_parent);
        // |failure_candidate| is invalid and we can't proceed further since we
        // have reached the root. Hence the longest proper suffix of this string
        // represented by this node is the empty string (represented by root).
        failure_candidate_id = kRootID;
      }

      child->SetFailure(failure_candidate_id);

      const AhoCorasickNode* failure_candidate = &tree_[failure_candidate_id];
      // Now |failure_candidate| is |child|'s longest possible proper suffix in
      // the trie. We also know that since we are doing a breadth first search,
      // we would have established |failure_candidate|'s output link by now.
      // Hence we can define |child|'s output link as follows:
      child->SetOutputLink(failure_candidate->IsEndOfPattern()
                               ? failure_candidate_id
                               : failure_candidate->output_link());

      queue.push(child);
    }
  }
}

void SubstringSetMatcher::AccumulateMatchesForNode(
    const AhoCorasickNode* node,
    std::set<StringPattern::ID>* matches) const {
  DCHECK(matches);

  if (node->IsEndOfPattern())
    matches->insert(node->GetMatchID());

  NodeID node_id = node->output_link();
  while (node_id != kInvalidNodeID) {
    node = &tree_[node_id];
    matches->insert(node->GetMatchID());
    node_id = node->output_link();
  }
}

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode() = default;
SubstringSetMatcher::AhoCorasickNode::~AhoCorasickNode() = default;

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode(AhoCorasickNode&& other) =
    default;

SubstringSetMatcher::AhoCorasickNode& SubstringSetMatcher::AhoCorasickNode::
operator=(AhoCorasickNode&& other) = default;

SubstringSetMatcher::NodeID SubstringSetMatcher::AhoCorasickNode::GetEdge(
    char c) const {
  auto i = edges_.find(c);
  return i == edges_.end() ? kInvalidNodeID : i->second;
}

void SubstringSetMatcher::AhoCorasickNode::SetEdge(char c, NodeID node) {
  DCHECK_NE(kInvalidNodeID, node);
  edges_[c] = node;
}

void SubstringSetMatcher::AhoCorasickNode::SetFailure(NodeID node) {
  DCHECK_NE(kInvalidNodeID, node);
  failure_ = node;
}

size_t SubstringSetMatcher::AhoCorasickNode::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(edges_);
}

}  // namespace url_matcher
