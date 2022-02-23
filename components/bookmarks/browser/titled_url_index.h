// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_INDEX_H_
#define COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_INDEX_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/bookmarks/browser/titled_url_node_sorter.h"
#include "components/query_parser/query_parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace bookmarks {

class TitledUrlNode;

struct TitledUrlMatch;

// TitledUrlIndex maintains an index of paired titles and URLs for quick lookup.
//
// TitledUrlIndex maintains the index (index_) as a map of sets. The map (type
// Index) maps from a lower case string to the set (type TitledUrlNodeSet) of
// TitledUrlNodes that contain that string in their title or URL.
class TitledUrlIndex {
 public:
  using TitledUrlNodeSet = base::flat_set<const TitledUrlNode*>;

  // Constructs a TitledUrlIndex. |sorter| is used to construct a sorted list
  // of matches when matches are returned from the index. If null, matches are
  // returned unsorted.
  explicit TitledUrlIndex(
      std::unique_ptr<TitledUrlNodeSorter> sorter = nullptr);

  TitledUrlIndex(const TitledUrlIndex&) = delete;
  TitledUrlIndex& operator=(const TitledUrlIndex&) = delete;

  ~TitledUrlIndex();

  void SetNodeSorter(std::unique_ptr<TitledUrlNodeSorter> sorter);

  // Invoked when a title/URL pair has been added to the model.
  void Add(const TitledUrlNode* node);

  // Invoked when a title/URL pair has been removed from the model.
  void Remove(const TitledUrlNode* node);

  // Returns up to |max_count| of matches containing each term from the text
  // |query| in either the title, URL, or, if |match_ancestor_titles| is true,
  // the titles of ancestor nodes. |matching_algorithm| determines the algorithm
  // used by QueryParser internally to parse |query|.
  std::vector<TitledUrlMatch> GetResultsMatching(
      const std::u16string& query,
      size_t max_count,
      query_parser::MatchingAlgorithm matching_algorithm,
      bool match_ancestor_titles);

  // For testing only.
  TitledUrlNodeSet RetrieveNodesMatchingAllTermsForTesting(
      const std::vector<std::u16string>& terms,
      query_parser::MatchingAlgorithm matching_algorithm) const {
    return RetrieveNodesMatchingAllTerms(terms, matching_algorithm);
  }

  // For testing only.
  TitledUrlNodeSet RetrieveNodesMatchingAnyTermsForTesting(
      const std::vector<std::u16string>& terms,
      query_parser::MatchingAlgorithm matching_algorithm,
      size_t max_nodes) const {
    return RetrieveNodesMatchingAnyTerms(terms, matching_algorithm, max_nodes);
  }

 private:
  using TitledUrlNodes = std::vector<const TitledUrlNode*>;
  using Index = std::map<std::u16string, TitledUrlNodeSet>;

  // Constructs |sorted_nodes| by copying the matches in |matches| and sorting
  // them.
  void SortMatches(const TitledUrlNodeSet& matches,
                   TitledUrlNodes* sorted_nodes) const;

  // For each node, calls `MatchTitledUrlNodeWithQuery()` and returns the
  // aggregated `TitledUrlMatch`s.
  std::vector<TitledUrlMatch> MatchTitledUrlNodesWithQuery(
      const TitledUrlNodes& nodes,
      const query_parser::QueryNodeVector& query_nodes,
      size_t max_count,
      bool match_ancestor_titles);

  // Finds |query_nodes| matches in |node| and returns a TitledUrlMatch
  // containing |node| and the matches.
  absl::optional<TitledUrlMatch> MatchTitledUrlNodeWithQuery(
      const TitledUrlNode* node,
      const query_parser::QueryNodeVector& query_nodes,
      bool match_ancestor_titles);

  // Return matches for the specified |terms|. This is an intersection of each
  // term's matches.
  TitledUrlNodeSet RetrieveNodesMatchingAllTerms(
      const std::vector<std::u16string>& terms,
      query_parser::MatchingAlgorithm matching_algorithm) const;

  // Return matches for the specified `terms`. This is approximately a union of
  // each term's match, with some limitations to avoid too many nodes being
  // returned: terms shorter than `term_min_length` or matching more than
  // `max_nodes_per_term` nodes won't have their nodes accumulated by union; and
  // accumulation is capped to `max_nodes`. Guaranteed to include any node
  // `RetrieveNodesMatchingAllTerms()` includes.
  TitledUrlNodeSet RetrieveNodesMatchingAnyTerms(
      const std::vector<std::u16string>& terms,
      query_parser::MatchingAlgorithm matching_algorithm,
      size_t max_nodes) const;

  // Return matches for the specified |term|. May return duplicates.
  TitledUrlNodes RetrieveNodesMatchingTerm(
      const std::u16string& term,
      query_parser::MatchingAlgorithm matching_algorithm) const;

  // Returns the set of query words from |query|.
  static std::vector<std::u16string> ExtractQueryWords(
      const std::u16string& query);

  // Return the index terms for |node|.
  static std::vector<std::u16string> ExtractIndexTerms(
      const TitledUrlNode* node);

  // Adds |node| to |index_|.
  void RegisterNode(const std::u16string& term, const TitledUrlNode* node);

  // Removes |node| from |index_|.
  void UnregisterNode(const std::u16string& term, const TitledUrlNode* node);

  Index index_;

  std::unique_ptr<TitledUrlNodeSorter> sorter_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_INDEX_H_
