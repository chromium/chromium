// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_PARSER_QUERY_PARSER_H_
#define COMPONENTS_QUERY_PARSER_QUERY_PARSER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "components/query_parser/snippet.h"

namespace query_parser {

class QueryNodeList;

// Used by HasMatchIn.
struct QueryWord {
  // The word to match against.
  std::u16string word;

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

// `QueryNode` is used by `QueryParser` to represent the elements that
// constitute a query. While `QueryNode` is exposed by way of `ParseQuery`, it
// really isn't meant for external usage.
class QueryNode {
 public:
  virtual ~QueryNode() = default;

  // Serialize ourselves out to a string that can be passed to SQLite. Returns
  // the number of words in this node.
  virtual int AppendToSQLiteQuery(std::u16string* query) const = 0;

  // Return true if this is a `QueryNodeWord`, false if it's a `QueryNodeList`.
  virtual bool IsWord() const = 0;

  // Returns true if this node matches `word`. If `exact` is true, the string
  // must exactly match. Otherwise, this uses a starts-with comparison.
  virtual bool Matches(const std::u16string& word, bool exact) const = 0;

  // Returns true if this node matches at least one of the words in `words`. An
  // entry is added to `match_positions` for all matching words giving the
  // matching regions. Uses a starts-with comparison.
  virtual bool HasMatchIn(const QueryWordVector& words,
                          Snippet::MatchPositions* match_positions) const = 0;

  // Returns true if this node matches at least one of the words in `words`.
  // If `exact` is true, at least one of the words must be an exact match.
  virtual bool HasMatchIn(const QueryWordVector& words, bool exact) const = 0;

  // Appends the words that make up this node in `words`.
  virtual void AppendWords(std::vector<std::u16string>* words) const = 0;
};

using QueryNodeVector = std::vector<std::unique_ptr<query_parser::QueryNode>>;

// This class is used to parse queries entered into the history search into more
// normalized queries that can be passed to the SQLite backend.
class QueryParser {
 public:
  QueryParser() = delete;

  QueryParser(const QueryParser&) = delete;
  QueryParser& operator=(const QueryParser&) = delete;

  ~QueryParser() = delete;

  // For CJK ideographs and Korean Hangul, even a single character
  // can be useful in prefix matching, but that may give us too many
  // false positives. Moreover, the current ICU word breaker gives us
  // back every single Chinese character as a word so that there's no
  // point doing anything for them and we only adjust the minimum length
  // to 2 for Korean Hangul while using 3 for others. This is a temporary
  // hack until we have a segmentation support.
  static bool IsWordLongEnoughForPrefixSearch(
      const std::u16string& word,
      MatchingAlgorithm matching_algorithm);

  // Parse a query into a SQLite query. The resulting query is placed in
  // |sqlite_query| and the number of words is returned.
  static int ParseQuery(const std::u16string& query,
                        MatchingAlgorithm matching_algorithm,
                        std::u16string* sqlite_query);

  // Parses |query|, returning the words that make up it. Any words in quotes
  // are put in |words| without the quotes. For example, the query text
  // "foo bar" results in two entries being added to words, one for foo and one
  // for bar.
  static void ParseQueryWords(const std::u16string& query,
                              MatchingAlgorithm matching_algorithm,
                              std::vector<std::u16string>* words);

  // Parses |query|, returning the nodes that constitute the valid words in the
  // query. This is intended for later usage with DoesQueryMatch. Ownership of
  // the nodes passes to the caller.
  static void ParseQueryNodes(const std::u16string& query,
                              MatchingAlgorithm matching_algorithm,
                              QueryNodeVector* nodes);

  // Returns true if all of the |find_nodes| are found in |find_in_text|.
  // |find_nodes| should have been created by calling |ParseQuery()|. If all
  // nodes were successfully found, each of the matching positions in the text
  // is added to |match_positions|.
  static bool DoesQueryMatch(const std::u16string& find_in_text,
                             const QueryNodeVector& find_nodes,
                             Snippet::MatchPositions* match_positions);

  // Returns true if all of the |find_nodes| are found in |find_in_words|.
  // |find_nodes| should have been created by calling |ParseQuery()|.
  // If |exact| is set to true, only exact matches are considered matches.
  static bool DoesQueryMatch(const QueryWordVector& find_in_words,
                             const QueryNodeVector& find_nodes,
                             bool exact = false);

  // Extracts the words from |text|, placing each word into |words|.
  // |text| must already be lowercased by the caller, as otherwise the output
  // will NEVER match anything.
  static void ExtractQueryWords(const std::u16string& text,
                                QueryWordVector* words);

  // Sorts the match positions in |matches| by their first index, then
  // coalesces any match positions that intersect each other.
  static void SortAndCoalesceMatchPositions(Snippet::MatchPositions* matches);

 private:
  // Does the work of parsing |query|; creates nodes in |root| as appropriate.
  // This is invoked from both of the ParseQuery methods.
  static bool ParseQueryImpl(const std::u16string& query,
                             MatchingAlgorithm matching_algorithm,
                             QueryNodeList* root);
};

}  // namespace query_parser

#endif  // COMPONENTS_QUERY_PARSER_QUERY_PARSER_H_
