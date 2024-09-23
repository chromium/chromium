// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_INDEX_H_
#define COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_INDEX_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/titled_url_node_sorter.h"
#include "components/query_parser/query_parser.h"

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
  using TitledUrlNodeSet =
      base::flat_set<raw_ptr<const TitledUrlNode, CtnExperimental>>;

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

  // Invoked when a folder has been added to the model.
  void AddPath(const TitledUrlNode* node);

  // Invoked when a folder has been removed from the model.
  void RemovePath(const TitledUrlNode* node);

  // Returns up to `max_count` of matches containing each term from the text
  // `query` in either the title, URL, or the titles of ancestor nodes.
  // `matching_algorithm` determines the algorithm used by QueryParser
  // internally to parse `query`.
  std::vector<TitledUrlMatch> GetResultsMatching(
      const std::u16string& query,
      size_t max_count,
      query_parser::MatchingAlgorithm matching_algorithm);

  // Returns a normalized version of the UTF16 string `text`.  If it fails to
  // normalize the string, returns `text` itself as a best-effort.
  static std::u16string Normalize(std::u16string_view text);

 private:
  friend class TitledUrlIndexFake;

  using TitledUrlNodes =
      std::vector<raw_ptr<const TitledUrlNode, CtnExperimental>>;
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
      const std::vector<std::u16string>& query_terms,
      size_t max_count);

  // Finds |query_nodes| matches in |node| and returns a TitledUrlMatch
  // containing |node| and the matches.
  std::optional<TitledUrlMatch> MatchTitledUrlNodeWithQuery(
      const TitledUrlNode* node,
      const query_parser::QueryNodeVector& query_nodes,
      const std::vector<std::u16string>& query_terms);

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

  // Return true if `term` matches any path. in `path_index_`.
  bool DoesTermMatchPath(
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

  // A map of terms and the nodes containing those terms in their titles or
  // URLs. E.g., given 2 bookmarks titled 'x y x' and 'x z', `index` would
  // contain: `{ x: set[node1, node2], y: set[node1], z: set[node2] }`.
  Index index_;
  // A map of terms and the number of times it occurs in paths. E.g., given
  // 2 paths 'bookmarks bar/x y x/x' and 'bookmarks bar/x z/x', `path_index_`
  // would contain `{ bookmarks: 2, bar: 2, x: 4, y: 1, z: 1 }`. Note, 'x' has
  // count 4, since it occurred twice in each path. Doesn't track actual
  // bookmark nodes, as the latter would need large updates when moving,
  // folders. Tracks counts so terms can be unindexed when the last containing
  // folder is renamed or deleted. Updated on folder rename, creation, and
  // deletion; not updated on bookmark or folder move. Used to short circuit
  // unioning per-term matches when matching paths, as intersecting results in
  // much fewer nodes.
  std::map<std::u16string, size_t> path_index_;

  std::unique_ptr<TitledUrlNodeSorter> sorter_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_INDEX_H_
