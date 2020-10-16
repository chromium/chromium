// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_INDEX_H_
#define COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_INDEX_H_

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
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
  // Constructs a TitledUrlIndex. |sorter| is used to construct a sorted list
  // of matches when matches are returned from the index. If null, matches are
  // returned unsorted.
  explicit TitledUrlIndex(
      std::unique_ptr<TitledUrlNodeSorter> sorter = nullptr);
  ~TitledUrlIndex();

  void SetNodeSorter(std::unique_ptr<TitledUrlNodeSorter> sorter);

  // Invoked when a title/URL pair has been added to the model.
  void Add(const TitledUrlNode* node);

  // Invoked when a title/URL pair has been removed from the model.
  void Remove(const TitledUrlNode* node);

  // Returns up to |max_count| of matches containing each term from the text
  // |query| in either the title or the URL.
  std::vector<TitledUrlMatch> GetResultsMatching(
      const base::string16& query,
      size_t max_count,
      query_parser::MatchingAlgorithm matching_algorithm);

 private:
  using TitledUrlNodes = std::vector<const TitledUrlNode*>;
  using TitledUrlNodeSet = base::flat_set<const TitledUrlNode*>;
  using Index = std::map<base::string16, TitledUrlNodeSet>;

  // Constructs |sorted_nodes| by copying the matches in |matches| and sorting
  // them.
  void SortMatches(const TitledUrlNodeSet& matches,
                   TitledUrlNodes* sorted_nodes) const;

  // Finds |query_nodes| matches in |node| and returns a TitledUrlMatch
  // containing |node| and the matches.
  base::Optional<TitledUrlMatch> MatchTitledUrlNodeWithQuery(
      const TitledUrlNode* node,
      query_parser::QueryParser* parser,
      const query_parser::QueryNodeVector& query_nodes);

  // Populates |matches| for the specified term. If |first_term| is true, this
  // is the first term in the query. Returns true if there is at least one node
  // matching the term.
  bool GetResultsMatchingTerm(
      const base::string16& term,
      bool first_term,
      query_parser::MatchingAlgorithm matching_algorithm,
      TitledUrlNodeSet* matches);

  // Returns the set of query words from |query|.
  std::vector<base::string16> ExtractQueryWords(const base::string16& query);

  // Adds |node| to |index_|.
  void RegisterNode(const base::string16& term, const TitledUrlNode* node);

  // Removes |node| from |index_|.
  void UnregisterNode(const base::string16& term, const TitledUrlNode* node);

  Index index_;

  std::unique_ptr<TitledUrlNodeSorter> sorter_;

  DISALLOW_COPY_AND_ASSIGN(TitledUrlIndex);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_INDEX_H_
