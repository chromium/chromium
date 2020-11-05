// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/titled_url_index.h"

#include <stdint.h>

#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/query_parser/snippet.h"
#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace bookmarks {

namespace {

// Returns a normalized version of the UTF16 string |text|.  If it fails to
// normalize the string, returns |text| itself as a best-effort.
base::string16 Normalize(const base::string16& text) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* normalizer2 =
      icu::Normalizer2::getInstance(nullptr, "nfkc", UNORM2_COMPOSE, status);
  if (U_FAILURE(status)) {
    // Log and crash right away to capture the error code in the crash report.
    LOG(FATAL) << "failed to create a normalizer: " << u_errorName(status);
  }
  icu::UnicodeString unicode_text(
      text.data(), static_cast<int32_t>(text.length()));
  icu::UnicodeString unicode_normalized_text;
  normalizer2->normalize(unicode_text, unicode_normalized_text, status);
  if (U_FAILURE(status)) {
    // This should not happen. Log the error and fall back.
    LOG(ERROR) << "normalization failed: " << u_errorName(status);
    return text;
  }
  return base::i18n::UnicodeStringToString16(unicode_normalized_text);
}

}  // namespace

TitledUrlIndex::TitledUrlIndex(std::unique_ptr<TitledUrlNodeSorter> sorter)
    : sorter_(std::move(sorter)) {
}

TitledUrlIndex::~TitledUrlIndex() {
}

void TitledUrlIndex::SetNodeSorter(
    std::unique_ptr<TitledUrlNodeSorter> sorter) {
  sorter_ = std::move(sorter);
}

void TitledUrlIndex::Add(const TitledUrlNode* node) {
  for (const base::string16& term : ExtractIndexTerms(node))
    RegisterNode(term, node);
}

void TitledUrlIndex::Remove(const TitledUrlNode* node) {
  for (const base::string16& term : ExtractIndexTerms(node))
    UnregisterNode(term, node);
}

std::vector<TitledUrlMatch> TitledUrlIndex::GetResultsMatching(
    const base::string16& input_query,
    size_t max_count,
    query_parser::MatchingAlgorithm matching_algorithm,
    bool match_ancestor_titles) {
  const base::string16 query = Normalize(input_query);
  std::vector<base::string16> terms = ExtractQueryWords(query);

  // When |match_ancestor_titles| is true, |matches| shouldn't exclude nodes
  // that don't match every query term, as the query terms may match in the
  // ancestors. |MatchTitledUrlNodeWithQuery()| below will filter out nodes that
  // neither match nor ancestor-match every query term.
  TitledUrlNodeSet matches =
      match_ancestor_titles
          ? RetrieveNodesMatchingAnyTerms(terms, matching_algorithm)
          : RetrieveNodesMatchingAllTerms(terms, matching_algorithm);
  if (matches.empty())
    return {};

  TitledUrlNodes sorted_nodes;
  SortMatches(matches, &sorted_nodes);

  // We use a QueryParser to fill in match positions for us. It's not the most
  // efficient way to go about this, but by the time we get here we know what
  // matches and so this shouldn't be performance critical.
  query_parser::QueryParser parser;
  query_parser::QueryNodeVector query_nodes;
  parser.ParseQueryNodes(query, matching_algorithm, &query_nodes);

  // The highest typed counts should be at the beginning of the results vector
  // so that the best matches will always be included in the results. The loop
  // that calculates result relevance in HistoryContentsProvider::ConvertResults
  // will run backwards to assure higher relevance will be attributed to the
  // best matches.
  std::vector<TitledUrlMatch> results;
  for (TitledUrlNodes::const_iterator i = sorted_nodes.begin();
       i != sorted_nodes.end() && results.size() < max_count; ++i) {
    base::Optional<TitledUrlMatch> match = MatchTitledUrlNodeWithQuery(
        *i, &parser, query_nodes, match_ancestor_titles);
    if (match)
      results.push_back(match.value());
  }
  return results;
}

void TitledUrlIndex::SortMatches(const TitledUrlNodeSet& matches,
                                 TitledUrlNodes* sorted_nodes) const {
  if (sorter_) {
    sorter_->SortMatches(matches, sorted_nodes);
  } else {
    sorted_nodes->insert(sorted_nodes->end(), matches.begin(), matches.end());
  }
}

base::Optional<TitledUrlMatch> TitledUrlIndex::MatchTitledUrlNodeWithQuery(
    const TitledUrlNode* node,
    query_parser::QueryParser* parser,
    const query_parser::QueryNodeVector& query_nodes,
    bool match_ancestor_titles) {
  if (!node) {
    return base::nullopt;
  }
  // Check that the result matches the query.  The previous search
  // was a simple per-word search, while the more complex matching
  // of QueryParser may filter it out.  For example, the query
  // ["thi"] will match the title [Thinking], but since
  // ["thi"] is quoted we don't want to do a prefix match.
  query_parser::QueryWordVector title_words, url_words, ancestor_words;
  const base::string16 lower_title =
      base::i18n::ToLower(Normalize(node->GetTitledUrlNodeTitle()));
  parser->ExtractQueryWords(lower_title, &title_words);
  base::OffsetAdjuster::Adjustments adjustments;
  parser->ExtractQueryWords(
      CleanUpUrlForMatching(node->GetTitledUrlNodeUrl(), &adjustments),
      &url_words);
  if (match_ancestor_titles) {
    for (auto ancestor : node->GetTitledUrlNodeAncestorTitles()) {
      parser->ExtractQueryWords(
          base::i18n::ToLower(Normalize(base::string16(ancestor))),
          &ancestor_words);
    }
  }

  query_parser::Snippet::MatchPositions title_matches, url_matches;
  bool query_has_ancestor_matches = false;
  for (const auto& node : query_nodes) {
    const bool has_title_matches =
        node->HasMatchIn(title_words, &title_matches);
    const bool has_url_matches = node->HasMatchIn(url_words, &url_matches);
    const bool has_ancestor_matches =
        match_ancestor_titles && node->HasMatchIn(ancestor_words);
    query_has_ancestor_matches =
        query_has_ancestor_matches || has_ancestor_matches;
    if (!has_title_matches && !has_url_matches && !has_ancestor_matches)
      return base::nullopt;
    query_parser::QueryParser::SortAndCoalesceMatchPositions(&title_matches);
    query_parser::QueryParser::SortAndCoalesceMatchPositions(&url_matches);
  }

  TitledUrlMatch match;
  if (lower_title.length() == node->GetTitledUrlNodeTitle().length()) {
    // Only use title matches if the lowercase string is the same length
    // as the original string, otherwise the matches are meaningless.
    // TODO(mpearson): revise match positions appropriately.
    match.title_match_positions.swap(title_matches);
  }
  // Now that we're done processing this entry, correct the offsets of the
  // matches in |url_matches| so they point to offsets in the original URL
  // spec, not the cleaned-up URL string that we used for matching.
  std::vector<size_t> offsets =
      TitledUrlMatch::OffsetsFromMatchPositions(url_matches);
  base::OffsetAdjuster::UnadjustOffsets(adjustments, &offsets);
  url_matches =
      TitledUrlMatch::ReplaceOffsetsInMatchPositions(url_matches, offsets);
  match.url_match_positions.swap(url_matches);
  match.has_ancestor_match = query_has_ancestor_matches;
  match.node = node;
  return match;
}

TitledUrlIndex::TitledUrlNodeSet TitledUrlIndex::RetrieveNodesMatchingAllTerms(
    const std::vector<base::string16>& terms,
    query_parser::MatchingAlgorithm matching_algorithm) const {
  if (terms.empty())
    return {};

  TitledUrlNodeSet matches =
      RetrieveNodesMatchingTerm(terms[0], matching_algorithm);
  for (size_t i = 1; i < terms.size() && !matches.empty(); ++i) {
    TitledUrlNodeSet term_matches =
        RetrieveNodesMatchingTerm(terms[i], matching_algorithm);
    // Compute intersection between the two sets.
    base::EraseIf(matches, base::IsNotIn<TitledUrlNodeSet>(term_matches));
  }

  return matches;
}

TitledUrlIndex::TitledUrlNodeSet TitledUrlIndex::RetrieveNodesMatchingAnyTerms(
    const std::vector<base::string16>& terms,
    query_parser::MatchingAlgorithm matching_algorithm) const {
  if (terms.empty())
    return {};

  TitledUrlNodes matches =
      RetrieveNodesMatchingTerm(terms[0], matching_algorithm);
  for (size_t i = 1; i < terms.size(); ++i) {
    TitledUrlNodes term_matches =
        RetrieveNodesMatchingTerm(terms[i], matching_algorithm);
    std::copy(term_matches.begin(), term_matches.end(),
              std::back_inserter(matches));
  }

  return TitledUrlNodeSet(matches);
}

TitledUrlIndex::TitledUrlNodes TitledUrlIndex::RetrieveNodesMatchingTerm(
    const base::string16& term,
    query_parser::MatchingAlgorithm matching_algorithm) const {
  Index::const_iterator i = index_.lower_bound(term);
  if (i == index_.end())
    return {};

  if (!query_parser::QueryParser::IsWordLongEnoughForPrefixSearch(
      term, matching_algorithm)) {
    // Term is too short for prefix match, compare using exact match.
    if (i->first != term)
      return {};  // No title/URL pairs with this term.
    return TitledUrlNodes(i->second.begin(), i->second.end());
  }

  // Loop through index adding all entries that start with term to
  // |prefix_matches|.
  TitledUrlNodes prefix_matches;
  while (i != index_.end() && i->first.size() >= term.size() &&
         term.compare(0, term.size(), i->first, 0, term.size()) == 0) {
    prefix_matches.insert(prefix_matches.end(), i->second.begin(),
                          i->second.end());
    ++i;
  }
  return prefix_matches;
}

// static
std::vector<base::string16> TitledUrlIndex::ExtractQueryWords(
    const base::string16& query) {
  std::vector<base::string16> terms;
  if (query.empty())
    return std::vector<base::string16>();
  query_parser::QueryParser parser;
  parser.ParseQueryWords(base::i18n::ToLower(query),
                         query_parser::MatchingAlgorithm::DEFAULT,
                         &terms);
  return terms;
}

// static
std::vector<base::string16> TitledUrlIndex::ExtractIndexTerms(
    const TitledUrlNode* node) {
  std::vector<base::string16> terms;

  for (const base::string16& term :
       ExtractQueryWords(Normalize(node->GetTitledUrlNodeTitle()))) {
    terms.push_back(term);
  }

  for (const base::string16& term : ExtractQueryWords(CleanUpUrlForMatching(
           node->GetTitledUrlNodeUrl(), /*adjustments=*/nullptr))) {
    terms.push_back(term);
  }

  return terms;
}

void TitledUrlIndex::RegisterNode(const base::string16& term,
                                 const TitledUrlNode* node) {
  index_[term].insert(node);
}

void TitledUrlIndex::UnregisterNode(const base::string16& term,
                                   const TitledUrlNode* node) {
  auto i = index_.find(term);
  if (i == index_.end()) {
    // We can get here if the node has the same term more than once. For
    // example, a node with the title 'foo foo' would end up here.
    return;
  }
  i->second.erase(node);
  if (i->second.empty())
    index_.erase(i);
}

}  // namespace bookmarks
