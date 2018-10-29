// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/substring_set_matcher.h"

#include <stddef.h>

#include <algorithm>
#include <queue>

#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/stl_util.h"

namespace url_matcher {

namespace {

// Compare StringPattern instances based on their string patterns.
bool ComparePatterns(const StringPattern* a, const StringPattern* b) {
  return a->pattern() < b->pattern();
}

// Given the set of patterns, compute how many nodes will the corresponding
// Aho-Corasick tree have. Note that |patterns| need to be sorted.
uint32_t TreeSize(const std::vector<const StringPattern*>& patterns) {
  uint32_t result = 1u;  // 1 for the root node.
  if (patterns.empty())
    return result;

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
    const uint32_t prefix_bound =
        std::min(last_pattern.size(), current_pattern.size());

    uint32_t common_prefix = 0;
    while (common_prefix < prefix_bound &&
           last_pattern[common_prefix] == current_pattern[common_prefix])
      ++common_prefix;
    result += current_pattern.size() - common_prefix;
  }
  return result;
}

}  // namespace

//
// SubstringSetMatcher
//

SubstringSetMatcher::SubstringSetMatcher() {
  RebuildAhoCorasickTree(SubstringPatternVector());
}

SubstringSetMatcher::~SubstringSetMatcher() {}

void SubstringSetMatcher::RegisterPatterns(
    const std::vector<const StringPattern*>& patterns) {
  RegisterAndUnregisterPatterns(patterns,
                                std::vector<const StringPattern*>());
}

void SubstringSetMatcher::UnregisterPatterns(
    const std::vector<const StringPattern*>& patterns) {
  RegisterAndUnregisterPatterns(std::vector<const StringPattern*>(),
                                patterns);
}

void SubstringSetMatcher::RegisterAndUnregisterPatterns(
      const std::vector<const StringPattern*>& to_register,
      const std::vector<const StringPattern*>& to_unregister) {
  // Register patterns.
  for (auto i = to_register.begin(); i != to_register.end(); ++i) {
    DCHECK(patterns_.find((*i)->id()) == patterns_.end());
    patterns_[(*i)->id()] = *i;
  }

  // Unregister patterns
  for (auto i = to_unregister.begin(); i != to_unregister.end(); ++i) {
    patterns_.erase((*i)->id());
  }

  // Now we compute the total number of tree nodes needed.
  SubstringPatternVector sorted_patterns;
  sorted_patterns.resize(patterns_.size());

  size_t next = 0;
  for (SubstringPatternMap::const_iterator i = patterns_.begin();
       i != patterns_.end();
       ++i, ++next) {
    sorted_patterns[next] = i->second;
  }

  std::sort(sorted_patterns.begin(), sorted_patterns.end(), ComparePatterns);
  tree_.reserve(TreeSize(sorted_patterns));

  RebuildAhoCorasickTree(sorted_patterns);
}

bool SubstringSetMatcher::Match(const std::string& text,
                                std::set<StringPattern::ID>* matches) const {
  const size_t old_number_of_matches = matches->size();

  // Handle patterns matching the empty string.
  matches->insert(tree_[0].matches().begin(), tree_[0].matches().end());

  uint32_t current_node = 0;
  for (std::string::const_iterator i = text.begin(); i != text.end(); ++i) {
    uint32_t edge_from_current = tree_[current_node].GetEdge(*i);
    while (edge_from_current == AhoCorasickNode::kNoSuchEdge &&
           current_node != 0) {
      current_node = tree_[current_node].failure();
      edge_from_current = tree_[current_node].GetEdge(*i);
    }
    if (edge_from_current != AhoCorasickNode::kNoSuchEdge) {
      current_node = edge_from_current;
      matches->insert(tree_[current_node].matches().begin(),
                      tree_[current_node].matches().end());
    } else {
      DCHECK_EQ(0u, current_node);
    }
  }

  return old_number_of_matches != matches->size();
}

bool SubstringSetMatcher::IsEmpty() const {
  // An empty tree consists of only the root node.
  return patterns_.empty() && tree_.size() == 1u;
}

void SubstringSetMatcher::RebuildAhoCorasickTree(
    const SubstringPatternVector& sorted_patterns) {
  tree_.clear();

  // Initialize root note of tree.
  AhoCorasickNode root;
  root.set_failure(0);
  tree_.push_back(root);

  // Insert all patterns.
  for (auto i = sorted_patterns.begin(); i != sorted_patterns.end(); ++i) {
    InsertPatternIntoAhoCorasickTree(*i);
  }

  CreateFailureEdges();
}

void SubstringSetMatcher::InsertPatternIntoAhoCorasickTree(
    const StringPattern* pattern) {
  const std::string& text = pattern->pattern();
  const std::string::const_iterator text_end = text.end();

  // Iterators on the tree and the text.
  uint32_t current_node = 0;
  std::string::const_iterator i = text.begin();

  // Follow existing paths for as long as possible.
  while (i != text_end) {
    uint32_t edge_from_current = tree_[current_node].GetEdge(*i);
    if (edge_from_current == AhoCorasickNode::kNoSuchEdge)
      break;
    current_node = edge_from_current;
    ++i;
  }

  // Create new nodes if necessary.
  while (i != text_end) {
    tree_.push_back(AhoCorasickNode());
    tree_[current_node].SetEdge(*i, tree_.size() - 1);
    current_node = tree_.size() - 1;
    ++i;
  }

  // Register match.
  tree_[current_node].AddMatch(pattern->id());
}

void SubstringSetMatcher::CreateFailureEdges() {
  typedef AhoCorasickNode::Edges Edges;

  base::queue<uint32_t> queue;

  AhoCorasickNode& root = tree_[0];
  root.set_failure(0);
  const Edges& root_edges = root.edges();
  for (auto e = root_edges.begin(); e != root_edges.end(); ++e) {
    const uint32_t& leads_to = e->second;
    tree_[leads_to].set_failure(0);
    queue.push(leads_to);
  }

  while (!queue.empty()) {
    AhoCorasickNode& current_node = tree_[queue.front()];
    queue.pop();
    for (auto e = current_node.edges().begin(); e != current_node.edges().end();
         ++e) {
      const char& edge_label = e->first;
      const uint32_t& leads_to = e->second;
      queue.push(leads_to);

      uint32_t failure = current_node.failure();
      uint32_t edge_from_failure = tree_[failure].GetEdge(edge_label);
      while (edge_from_failure == AhoCorasickNode::kNoSuchEdge &&
             failure != 0) {
        failure = tree_[failure].failure();
        edge_from_failure = tree_[failure].GetEdge(edge_label);
      }

      const uint32_t follow_in_case_of_failure =
          edge_from_failure != AhoCorasickNode::kNoSuchEdge ? edge_from_failure
                                                            : 0;
      tree_[leads_to].set_failure(follow_in_case_of_failure);
      tree_[leads_to].AddMatches(tree_[follow_in_case_of_failure].matches());
    }
  }
}

const uint32_t SubstringSetMatcher::AhoCorasickNode::kNoSuchEdge = 0xFFFFFFFF;

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode()
    : failure_(kNoSuchEdge) {}

SubstringSetMatcher::AhoCorasickNode::~AhoCorasickNode() {}

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode(
    const SubstringSetMatcher::AhoCorasickNode& other)
    : edges_(other.edges_),
      failure_(other.failure_),
      matches_(other.matches_) {}

SubstringSetMatcher::AhoCorasickNode&
SubstringSetMatcher::AhoCorasickNode::operator=(
    const SubstringSetMatcher::AhoCorasickNode& other) {
  edges_ = other.edges_;
  failure_ = other.failure_;
  matches_ = other.matches_;
  return *this;
}

uint32_t SubstringSetMatcher::AhoCorasickNode::GetEdge(char c) const {
  auto i = edges_.find(c);
  return i == edges_.end() ? kNoSuchEdge : i->second;
}

void SubstringSetMatcher::AhoCorasickNode::SetEdge(char c, uint32_t node) {
  edges_[c] = node;
}

void SubstringSetMatcher::AhoCorasickNode::AddMatch(StringPattern::ID id) {
  matches_.insert(id);
}

void SubstringSetMatcher::AhoCorasickNode::AddMatches(
    const SubstringSetMatcher::AhoCorasickNode::Matches& matches) {
  matches_.insert(matches.begin(), matches.end());
}

}  // namespace url_matcher
