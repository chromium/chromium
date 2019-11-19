// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_PARSER_QUERY_PARSER_H_
#define COMPONENTS_QUERY_PARSER_QUERY_PARSER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/query_parser/snippet.h"

namespace query_parser {

class QueryNodeList;

// Used by HasMatchIn.
struct QueryWord {
  // The work to match against.
  base::string16 word;

  // The starting position of the word in the original text.
  size_t position;
};

enum class MatchingAlgorithm {
  // Only words long enough are considered for prefix search. Shorter words are
  // considered for exact matches.
  DEFAULT,
  // All words are considered for a prefix search.
  ALWAYS_PREFIX_SEARCH,
  kMaxValue = ALWAYS_PREFIX_SEARCH,
};

using QueryWordVector = std::vector<query_parser::QueryWord>;

// QueryNode is used by QueryParser to represent the elements that constitute a
// query. While QueryNode is exposed by way of ParseQuery, it really isn't meant
// for external usage.
class QueryNode {
 public:
  virtual ~QueryNode() {}

  // Serialize ourselves out to a string that can be passed to SQLite. Returns
  // the number of words in this node.
  virtual int AppendToSQLiteQuery(base::string16* query) const = 0;

  // Return true if this is a QueryNodeWord, false if it's a QueryNodeList.
  virtual bool IsWord() const = 0;

  // Returns true if this node matches |word|. If |exact| is true, the string
  // must exactly match. Otherwise, this uses a starts with comparison.
  virtual bool Matches(const base::string16& word, bool exact) const = 0;

  // Returns true if this node matches at least one of the words in |words|. An
  // entry is added to |match_positions| for all matching words giving the
  // matching regions.
  virtual bool HasMatchIn(const QueryWordVector& words,
                          Snippet::MatchPositions* match_positions) const = 0;

  // Returns true if this node matches at least one of the words in |words|.
  virtual bool HasMatchIn(const QueryWordVector& words) const = 0;

  // Appends the words that make up this node in |words|.
  virtual void AppendWords(std::vector<base::string16>* words) const = 0;
};

using QueryNodeVector = std::vector<std::unique_ptr<query_parser::QueryNode>>;

// This class is used to parse queries entered into the history search into more
// normalized queries that can be passed to the SQLite backend.
class QueryParser {
 public:
  QueryParser();

  // For CJK ideographs and Korean Hangul, even a single character
  // can be useful in prefix matching, but that may give us too many
  // false positives. Moreover, the current ICU word breaker gives us
  // back every single Chinese character as a word so that there's no
  // point doing anything for them and we only adjust the minimum length
  // to 2 for Korean Hangul while using 3 for others. This is a temporary
  // hack until we have a segmentation support.
  static bool IsWordLongEnoughForPrefixSearch(
      const base::string16& word,
      MatchingAlgorithm matching_algorithm);

  // Parse a query into a SQLite query. The resulting query is placed in
  // |sqlite_query| and the number of words is returned.
  int ParseQuery(const base::string16& query,
                 MatchingAlgorithm matching_algorithm,
                 base::string16* sqlite_query);

  // Parses |query|, returning the words that make up it. Any words in quotes
  // are put in |words| without the quotes. For example, the query text
  // "foo bar" results in two entries being added to words, one for foo and one
  // for bar.
  void ParseQueryWords(const base::string16& query,
                       MatchingAlgorithm matching_algorithm,
                       std::vector<base::string16>* words);

  // Parses |query|, returning the nodes that constitute the valid words in the
  // query. This is intended for later usage with DoesQueryMatch. Ownership of
  // the nodes passes to the caller.
  void ParseQueryNodes(const base::string16& query,
                       MatchingAlgorithm matching_algorithm,
                       QueryNodeVector* nodes);

  // Returns true if the string text matches the query nodes created by a call
  // to ParseQuery. If the query does match, each of the matching positions in
  // the text is added to |match_positions|.
  bool DoesQueryMatch(const base::string16& text,
                      const QueryNodeVector& nodes,
                      Snippet::MatchPositions* match_positions);

  // Returns true if all of the |words| match the query |nodes| created by a
  // call to ParseQuery.
  bool DoesQueryMatch(const QueryWordVector& words,
                      const QueryNodeVector& nodes);

  // Extracts the words from |text|, placing each word into |words|.
  void ExtractQueryWords(const base::string16& text,
                         QueryWordVector* words);

  // Sorts the match positions in |matches| by their first index, then
  // coalesces any match positions that intersect each other.
  static void SortAndCoalesceMatchPositions(Snippet::MatchPositions* matches);

 private:
  // Does the work of parsing |query|; creates nodes in |root| as appropriate.
  // This is invoked from both of the ParseQuery methods.
  bool ParseQueryImpl(const base::string16& query,
                      MatchingAlgorithm matching_algorithm,
                      QueryNodeList* root);

  DISALLOW_COPY_AND_ASSIGN(QueryParser);
};

}  // namespace query_parser

#endif  // COMPONENTS_QUERY_PARSER_QUERY_PARSER_H_
