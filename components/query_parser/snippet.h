// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module computes snippets of queries based on hits in the documents
// for display in history search results.

#ifndef COMPONENTS_QUERY_PARSER_SNIPPET_H__
#define COMPONENTS_QUERY_PARSER_SNIPPET_H__

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

namespace query_parser {

class Snippet {
 public:
  // Each MatchPosition is the [begin, end) positions of a match within a
  // string.
  typedef std::pair<size_t, size_t> MatchPosition;
  typedef std::vector<MatchPosition> MatchPositions;

  // Parses an offsets string as returned from a sqlite full text index. An
  // offsets string encodes information about why a row matched a text query.
  // The information is encoded in the string as a set of matches, where each
  // match consists of the column, term-number, location, and length of the
  // match. Each element of the match is separated by a space, as is each match
  // from other matches.
  //
  // This method adds the start and end of each match whose column is
  // column_num to match_positions. The pairs are ordered based on first,
  // with no overlapping elements.
  //
  // NOTE: the positions returned are in terms of UTF8 encoding. To convert the
  // offsets to UTF-16, use ConvertMatchPositionsToWide
  static void ExtractMatchPositions(const std::string& offsets_str,
                                    const std::string& column_num,
                                    MatchPositions* match_positions);

  // Converts match positions as returned from ExtractMatchPositions to be in
  // terms of a UTF-16 2-byte code unit.
  static void ConvertMatchPositionsToWide(
      const std::string& utf8_string,
      Snippet::MatchPositions* match_positions);

  Snippet();
  Snippet(const Snippet& other);
  Snippet(Snippet&& other) noexcept;
  ~Snippet();

  Snippet& operator=(const Snippet&);

  // Given |matches|, the match positions within |document|, compute the snippet
  // for the document.
  // Note that |document| is UTF-8 and the offsets in |matches| are byte
  // offsets.
  void ComputeSnippet(const MatchPositions& matches,
                      const std::string& document);

  const std::u16string& text() const { return text_; }
  const MatchPositions& matches() const { return matches_; }

  // Efficiently swaps the contents of this snippet with the other.
  void Swap(Snippet* other);

 private:
  // The text of the snippet.
  std::u16string text_;

  // The matches within text_.
  MatchPositions matches_;
};

}  // namespace query_parser

#endif  // COMPONENTS_QUERY_PARSER_SNIPPET_H__
