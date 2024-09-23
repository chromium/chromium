// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/titled_url_index.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <unordered_set>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/query_parser/snippet.h"
#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace bookmarks {

namespace {

// Return true if `prefix` is a prefix of `string`.
bool IsPrefix(const std::u16string& prefix, const std::u16string& string) {
  return prefix.size() <= string.size() &&
         prefix.compare(0, prefix.size(), string, 0, prefix.size()) == 0;
}

}  // namespace

TitledUrlIndex::TitledUrlIndex(std::unique_ptr<TitledUrlNodeSorter> sorter)
    : sorter_(std::move(sorter)) {}

TitledUrlIndex::~TitledUrlIndex() = default;

void TitledUrlIndex::SetNodeSorter(
    std::unique_ptr<TitledUrlNodeSorter> sorter) {
  sorter_ = std::move(sorter);
}

void TitledUrlIndex::Add(const TitledUrlNode* node) {
  for (const std::u16string& term : ExtractIndexTerms(node))
    RegisterNode(term, node);
}

void TitledUrlIndex::Remove(const TitledUrlNode* node) {
  for (const std::u16string& term : ExtractIndexTerms(node))
    UnregisterNode(term, node);
}

void TitledUrlIndex::AddPath(const TitledUrlNode* node) {
  for (const std::u16string& term :
       ExtractQueryWords(Normalize(node->GetTitledUrlNodeTitle()))) {
    path_index_[term]++;
  }
}

void TitledUrlIndex::RemovePath(const TitledUrlNode* node) {
  for (const std::u16string& term :
       ExtractQueryWords(Normalize(node->GetTitledUrlNodeTitle()))) {
    // `path_index_.count(term)` should be > 0, since nodes can't be
    // removed/renamed if they didn't exist to begin with. But some tests don't
    // fully load bookmarks so it's not `DCHECK`ed.
    if (path_index_.count(term) && !--path_index_[term])
      path_index_.erase(term);
  }
}

std::vector<TitledUrlMatch> TitledUrlIndex::GetResultsMatching(
    const std::u16string& input_query,
    size_t max_count,
    query_parser::MatchingAlgorithm matching_algorithm) {
  const std::u16string query = Normalize(input_query);
  std::vector<std::u16string> terms = ExtractQueryWords(query);
  if (terms.empty())
    return {};

  // `ExtractQueryWords()` splits on symbols like '@'. That's usually good; e.g.
  // it allows 'xyz@gmail' to match 'xyz gmail'. But for inputs starting with
  // '@', like '@h', the user's more likely wants to enter the history scope
  // search than to select a 'https://google.com' bookmark.
  if (query.starts_with('@') && query.size() >= 2 && terms[0].size() >= 1 &&
      query[1] == terms[0][0]) {
    return {};
  }

  // `matches` shouldn't exclude nodes that don't match every query term, as the
  // query terms may match in the ancestors. `MatchTitledUrlNodeWithQuery()`
  // below will filter out nodes that neither match nor ancestor-match every
  // query term.
  static const size_t kMaxNodes = 1000;
  TitledUrlNodeSet matches =
      RetrieveNodesMatchingAnyTerms(terms, matching_algorithm, kMaxNodes);

  if (matches.empty())
    return {};

  TitledUrlNodes sorted_nodes;
  SortMatches(matches, &sorted_nodes);

  // We use a QueryParser to fill in match positions for us. It's not the most
  // efficient way to go about this, but by the time we get here we know what
  // matches and so this shouldn't be performance critical.
  query_parser::QueryNodeVector query_nodes;
  query_parser::QueryParser::ParseQueryNodes(query, matching_algorithm,
                                             &query_nodes);

  return MatchTitledUrlNodesWithQuery(sorted_nodes, query_nodes, terms,
                                      max_count);
}

// static
std::u16string TitledUrlIndex::Normalize(std::u16string_view text) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* normalizer2 =
      icu::Normalizer2::getInstance(nullptr, "nfkc", UNORM2_COMPOSE, status);
  if (U_FAILURE(status)) {
    // Log and crash right away to capture the error code in the crash report.
    LOG(FATAL) << "failed to create a normalizer: " << u_errorName(status);
  }
  icu::UnicodeString unicode_text(text.data(),
                                  static_cast<int32_t>(text.length()));
  icu::UnicodeString unicode_normalized_text;
  normalizer2->normalize(unicode_text, unicode_normalized_text, status);
  if (U_FAILURE(status)) {
    // This should not happen. Log the error and fall back.
    LOG(ERROR) << "normalization failed: " << u_errorName(status);
    return std::u16string(text);
  }
  return base::i18n::UnicodeStringToString16(unicode_normalized_text);
}

void TitledUrlIndex::SortMatches(const TitledUrlNodeSet& matches,
                                 TitledUrlNodes* sorted_nodes) const {
  if (sorter_) {
    sorter_->SortMatches(matches, sorted_nodes);
  } else {
    sorted_nodes->insert(sorted_nodes->end(), matches.begin(), matches.end());
  }
}

std::vector<TitledUrlMatch> TitledUrlIndex::MatchTitledUrlNodesWithQuery(
    const TitledUrlNodes& nodes,
    const query_parser::QueryNodeVector& query_nodes,
    const std::vector<std::u16string>& query_terms,
    size_t max_count) {
  // The highest typed counts should be at the beginning of the `matches` vector
  // so that the best matches will always be included in the results. The loop
  // that calculates match relevance in
  // `HistoryContentsProvider::ConvertResults()` will run backwards to assure
  // higher relevance will be attributed to the best matches.
  std::vector<TitledUrlMatch> matches;
  for (TitledUrlNodes::const_iterator i = nodes.begin();
       i != nodes.end() && matches.size() < max_count; ++i) {
    std::optional<TitledUrlMatch> match =
        MatchTitledUrlNodeWithQuery(*i, query_nodes, query_terms);
    if (match)
      matches.emplace_back(std::move(match).value());
  }
  return matches;
}

std::optional<TitledUrlMatch> TitledUrlIndex::MatchTitledUrlNodeWithQuery(
    const TitledUrlNode* node,
    const query_parser::QueryNodeVector& query_nodes,
    const std::vector<std::u16string>& query_terms) {
  if (!node) {
    return std::nullopt;
  }
  // Check that the result matches the query.  The previous search
  // was a simple per-word search, while the more complex matching
  // of QueryParser may filter it out.  For example, the query
  // ["thi"] will match the title [Thinking], but since
  // ["thi"] is quoted we don't want to do a prefix match.

  // Clean up the title, URL, and ancestor titles in preparation for string
  // comparisons.
  const std::u16string lower_title =
      base::i18n::ToLower(Normalize(node->GetTitledUrlNodeTitle()));
  base::OffsetAdjuster::Adjustments adjustments;
  const std::u16string clean_url =
      CleanUpUrlForMatching(node->GetTitledUrlNodeUrl(), &adjustments);
  std::vector<std::u16string> lower_ancestor_titles;
  base::ranges::transform(
      node->GetTitledUrlNodeAncestorTitles(),
      std::back_inserter(lower_ancestor_titles),
      [](const auto& ancestor_title) {
        return base::i18n::ToLower(Normalize(std::u16string(ancestor_title)));
      });

  // Check if the input approximately matches the node. This is less strict than
  // the following check; it will return false positives. But it's also much
  // faster, so if it returns false, early exit and avoid the expensive
  // `ExtractQueryWords()` calls.
  bool approximate_match =
      base::ranges::all_of(query_terms, [&](const auto& word) {
        if (lower_title.find(word) != std::u16string::npos)
          return true;
        if (clean_url.find(word) != std::u16string::npos)
          return true;
        for (const auto& ancestor_title : lower_ancestor_titles) {
          if (ancestor_title.find(word) != std::u16string::npos)
            return true;
        }

        return false;
      });
  if (!approximate_match)
    return std::nullopt;

  // If `node` passed the approximate check above, to the more accurate check.
  query_parser::QueryWordVector title_words, url_words, ancestor_words;
  query_parser::QueryParser::ExtractQueryWords(clean_url, &url_words);
  query_parser::QueryParser::ExtractQueryWords(lower_title, &title_words);
  for (const auto& ancestor_title : lower_ancestor_titles) {
    query_parser::QueryParser::ExtractQueryWords(ancestor_title,
                                                 &ancestor_words);
  }

  query_parser::Snippet::MatchPositions title_matches, url_matches;
  bool query_has_ancestor_matches = false;
  for (const auto& query_node : query_nodes) {
    const bool has_title_matches =
        query_node->HasMatchIn(title_words, &title_matches);
    const bool has_url_matches =
        query_node->HasMatchIn(url_words, &url_matches);
    const bool has_ancestor_matches =
        query_node->HasMatchIn(ancestor_words, false);
    query_has_ancestor_matches =
        query_has_ancestor_matches || has_ancestor_matches;
    if (!has_title_matches && !has_url_matches && !has_ancestor_matches)
      return std::nullopt;
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
    const std::vector<std::u16string>& terms,
    query_parser::MatchingAlgorithm matching_algorithm) const {
  DCHECK(!terms.empty());
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
    const std::vector<std::u16string>& terms,
    query_parser::MatchingAlgorithm matching_algorithm,
    size_t max_nodes) const {
  DCHECK(!terms.empty());

  if (terms.size() == 1)
    return RetrieveNodesMatchingAllTerms(terms, matching_algorithm);

  // If any term does not match a path, short circuit the expensive union
  // and simply resort to the `RetrieveNodesMatchingAllTerms()` intersection
  // of the terms not matching any path. The results are guaranteed to be the
  // same, since all terms must either title, URL, or path match; but there'll
  // be much fewer nodes returned.
  std::vector<std::u16string> terms_not_path;
  base::ranges::copy_if(terms, std::back_inserter(terms_not_path),
                        [&](const std::u16string& term) {
                          return !DoesTermMatchPath(term, matching_algorithm);
                        });
  if (!terms_not_path.empty())
    return RetrieveNodesMatchingAllTerms(terms_not_path, matching_algorithm);

  std::vector<TitledUrlNodes> matches_per_term;
  bool some_term_had_empty_matches = false;
  for (const std::u16string& term : terms) {
    // Use `matching_algorithm`, as opposed to exact matching, to allow inputs
    // like 'myFolder goog' to match a 'google.com' bookmark in a 'myFolder'
    // folder.
    TitledUrlNodes term_matches =
        RetrieveNodesMatchingTerm(term, matching_algorithm);
    if (term_matches.empty())
      some_term_had_empty_matches = true;
    else
      matches_per_term.push_back(std::move(term_matches));
  }

  // Sort `matches_per_term` least frequent first. This prevents terms like
  // 'https', which match a lot of nodes, from wasting `max_nodes` capacity.
  base::ranges::sort(
      matches_per_term,
      [](size_t first, size_t second) { return first < second; },
      [](const auto& matches) { return matches.size(); });

  // Use an `unordered_set` to avoid potentially 1000's of linear time
  // insertions into the ordered `TitledUrlNodeSet` (i.e. `flat_set`).
  std::unordered_set<const TitledUrlNode*> matches;
  for (const auto& term_matches : matches_per_term) {
    for (const TitledUrlNode* node : term_matches) {
      matches.insert(node);
      if (matches.size() == max_nodes)
        break;
    }
    if (matches.size() == max_nodes)
      break;
  }

  // Append all nodes that match every input term. This is necessary because
  // with the `max_nodes` threshold above, it's possible that `matches` is
  // missing some nodes that match every input term. Since `matches_per_term[i]`
  // is a superset of the intersection of `matches_per_term`s, if
  // `matches_per_term[0].size() <= max_nodes`, all of `matches_per_term[0]`,
  // and therefore the intersection matches`, are already in `matches`.
  TitledUrlNodeSet all_term_matches;
  if (!some_term_had_empty_matches && matches_per_term[0].size() > max_nodes) {
    all_term_matches = matches_per_term[0];
    for (size_t i = 1; i < matches_per_term.size() && !all_term_matches.empty();
         ++i) {
      // Compute intersection between the two sets.
      base::EraseIf(all_term_matches,
                    base::IsNotIn<TitledUrlNodeSet>(matches_per_term[i]));
    }
    // `all_term_matches` is the intersection of each term's node matches; the
    // same as `RetrieveNodesMatchingAllTerms()`. We don't call the latter as a
    // performance optimization.
    DCHECK(all_term_matches ==
           RetrieveNodesMatchingAllTerms(terms, matching_algorithm));
  }

  matches.insert(all_term_matches.begin(), all_term_matches.end());
  return TitledUrlNodeSet(matches.begin(), matches.end());
}

TitledUrlIndex::TitledUrlNodes TitledUrlIndex::RetrieveNodesMatchingTerm(
    const std::u16string& term,
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
  while (i != index_.end() && IsPrefix(term, i->first)) {
    prefix_matches.insert(prefix_matches.end(), i->second.begin(),
                          i->second.end());
    ++i;
  }
  return prefix_matches;
}

bool TitledUrlIndex::DoesTermMatchPath(
    const std::u16string& term,
    query_parser::MatchingAlgorithm matching_algorithm) const {
  // Term is too short for prefix match, compare using exact match.
  if (!query_parser::QueryParser::IsWordLongEnoughForPrefixSearch(
          term, matching_algorithm)) {
    return path_index_.count(term) > 0;
  }
  // Otherwise, see if any path is prefixed by `term`.
  const auto i = path_index_.lower_bound(term);
  return i != path_index_.end() && IsPrefix(term, i->first);
}

// static
std::vector<std::u16string> TitledUrlIndex::ExtractQueryWords(
    const std::u16string& query) {
  std::vector<std::u16string> terms;
  if (query.empty())
    return std::vector<std::u16string>();
  query_parser::QueryParser::ParseQueryWords(
      base::i18n::ToLower(query), query_parser::MatchingAlgorithm::DEFAULT,
      &terms);
  return terms;
}

// static
std::vector<std::u16string> TitledUrlIndex::ExtractIndexTerms(
    const TitledUrlNode* node) {
  std::vector<std::u16string> terms;

  for (const std::u16string& term :
       ExtractQueryWords(Normalize(node->GetTitledUrlNodeTitle()))) {
    terms.push_back(term);
  }

  for (const std::u16string& term : ExtractQueryWords(CleanUpUrlForMatching(
           node->GetTitledUrlNodeUrl(), /*adjustments=*/nullptr))) {
    terms.push_back(term);
  }

  return terms;
}

void TitledUrlIndex::RegisterNode(const std::u16string& term,
                                  const TitledUrlNode* node) {
  index_[term].insert(node);
}

void TitledUrlIndex::UnregisterNode(const std::u16string& term,
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
