// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_MATCHER_SUBSTRING_SET_MATCHER_H_
#define COMPONENTS_URL_MATCHER_SUBSTRING_SET_MATCHER_H_

#include <stdint.h>

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/url_matcher/string_pattern.h"
#include "components/url_matcher/url_matcher_export.h"

namespace url_matcher {

// Class that store a set of string patterns and can find for a string S,
// which string patterns occur in S.
class URL_MATCHER_EXPORT SubstringSetMatcher {
 public:
  // Registers all |patterns|. Each pattern needs to have a unique ID and all
  // pattern strings must be unique.
  // Complexity:
  //    Let n = number of patterns.
  //    Let S = sum of pattern lengths.
  //    Let k = range of char. Generally 256.
  // Complexity = O(nlogn + S * logk)
  // nlogn comes from sorting the patterns.
  // log(k) comes from our usage of std::map to store edges.
  SubstringSetMatcher(const std::vector<StringPattern>& patterns);
  SubstringSetMatcher(std::vector<const StringPattern*> patterns);

  SubstringSetMatcher(const SubstringSetMatcher&) = delete;
  SubstringSetMatcher& operator=(const SubstringSetMatcher&) = delete;

  ~SubstringSetMatcher();

  // Matches |text| against all registered StringPatterns. Stores the IDs
  // of matching patterns in |matches|. |matches| is not cleared before adding
  // to it.
  // Complexity:
  //    Let t = length of |text|.
  //    Let k = range of char. Generally 256.
  //    Let z = number of matches returned.
  // Complexity = O(t * logk + zlogz)
  bool Match(const std::string& text,
             std::set<StringPattern::ID>* matches) const;

  // Returns true if this object retains no allocated data.
  bool IsEmpty() const { return is_empty_; }

  // Returns the dynamically allocated memory usage in bytes. See
  // base/trace_event/memory_usage_estimator.h for details.
  size_t EstimateMemoryUsage() const;

 private:
  // Represents the index of the node within |tree_|. It is specifically
  // uint32_t so that we can be sure it takes up 4 bytes. If the computed size
  // of |tree_| is larger than what can be stored within an uint32_t, there will
  // be a CHECK failure.
  using NodeID = uint32_t;

  // This is the maximum possible size of |tree_| and hence can't be a valid ID.
  static constexpr NodeID kInvalidNodeID = std::numeric_limits<NodeID>::max();

  static constexpr NodeID kRootID = 0;

  // A node of an Aho Corasick Tree. See
  // http://web.stanford.edu/class/archive/cs/cs166/cs166.1166/lectures/02/Small02.pdf
  // to understand the algorithm.
  //
  // The algorithm is based on the idea of building a trie of all registered
  // patterns. Each node of the tree is annotated with a set of pattern
  // IDs that are used to report matches.
  //
  // The root of the trie represents an empty match. If we were looking whether
  // any registered pattern matches a text at the beginning of the text (i.e.
  // whether any pattern is a prefix of the text), we could just follow
  // nodes in the trie according to the matching characters in the text.
  // E.g., if text == "foobar", we would follow the trie from the root node
  // to its child labeled 'f', from there to child 'o', etc. In this process we
  // would report all pattern IDs associated with the trie nodes as matches.
  //
  // As we are not looking for all prefix matches but all substring matches,
  // this algorithm would need to compare text.substr(0), text.substr(1), ...
  // against the trie, which is in O(|text|^2).
  //
  // The Aho Corasick algorithm improves this runtime by using failure edges.
  // In case we have found a partial match of length k in the text
  // (text[i, ..., i + k - 1]) in the trie starting at the root and ending at
  // a node at depth k, but cannot find a match in the trie for character
  // text[i + k] at depth k + 1, we follow a failure edge. This edge
  // corresponds to the longest proper suffix of text[i, ..., i + k - 1] that
  // is a prefix of any registered pattern.
  //
  // If your brain thinks "Forget it, let's go shopping.", don't worry.
  // Take a nap and read an introductory text on the Aho Corasick algorithm.
  // It will make sense. Eventually.
  class AhoCorasickNode {
   public:
    // Map from edge label to NodeID.
    using Edges = base::flat_map<char, NodeID>;

    AhoCorasickNode();
    ~AhoCorasickNode();
    AhoCorasickNode(AhoCorasickNode&& other);
    AhoCorasickNode& operator=(AhoCorasickNode&& other);

    NodeID GetEdge(char c) const;
    void SetEdge(char c, NodeID node);
    const Edges& edges() const { return edges_; }

    void ShrinkEdges() { edges_.shrink_to_fit(); }

    NodeID failure() const { return failure_; }
    void SetFailure(NodeID failure);

    void SetMatchID(StringPattern::ID id) {
      DCHECK(!IsEndOfPattern());
      match_id_ = id;
    }

    // Returns true if this node corresponds to a pattern.
    bool IsEndOfPattern() const {
      return match_id_ != StringPattern::kInvalidId;
    }

    // Must only be called if |IsEndOfPattern| returns true for this node.
    StringPattern::ID GetMatchID() const {
      DCHECK(IsEndOfPattern());
      return match_id_;
    }

    void SetOutputLink(NodeID node) { output_link_ = node; }
    NodeID output_link() const { return output_link_; }

    size_t EstimateMemoryUsage() const;

   private:
    // Outgoing edges of current node.
    Edges edges_;

    // Node index that failure edge leads to. The failure node corresponds to
    // the node which represents the longest proper suffix (include empty
    // string) of the string represented by this node. Must be valid, equal to
    // kInvalidNodeID when uninitialized.
    NodeID failure_ = kInvalidNodeID;

    // If valid, this node represents the end of a pattern. It stores the ID of
    // the corresponding pattern.
    StringPattern::ID match_id_ = StringPattern::kInvalidId;

    // Node index that corresponds to the longest proper suffix (including empty
    // suffix) of this node and which also represents the end of a pattern. Can
    // be invalid.
    NodeID output_link_ = kInvalidNodeID;
  };

  using SubstringPatternVector = std::vector<const StringPattern*>;

  // Given the set of patterns, compute how many nodes will the corresponding
  // Aho-Corasick tree have. Note that |patterns| need to be sorted.
  NodeID GetTreeSize(const std::vector<const StringPattern*>& patterns) const;

  void BuildAhoCorasickTree(const SubstringPatternVector& patterns);

  // Inserts a path for |pattern->pattern()| into the tree and adds
  // |pattern->id()| to the set of matches.
  void InsertPatternIntoAhoCorasickTree(const StringPattern* pattern);

  void CreateFailureAndOutputEdges();

  // Adds all pattern IDs to |matches| which are a suffix of the string
  // represented by |node|.
  void AccumulateMatchesForNode(const AhoCorasickNode* node,
                                std::set<StringPattern::ID>* matches) const;

  // The nodes of a Aho-Corasick tree.
  std::vector<AhoCorasickNode> tree_;

  bool is_empty_ = true;
};

}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_SUBSTRING_SET_MATCHER_H_
