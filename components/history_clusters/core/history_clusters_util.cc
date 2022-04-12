// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_util.h"

#include <algorithm>

#include "base/i18n/case_conversion.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/visitsegment_database.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/query_parser/query_parser.h"
#include "components/url_formatter/url_formatter.h"

namespace history_clusters {

namespace {

// Returns true if `find_nodes` matches `cluster_keywords`.
bool DoesQueryMatchClusterKeywords(
    const query_parser::QueryNodeVector& find_nodes,
    const std::vector<std::u16string>& cluster_keywords) {
  // All of the cluster's `keyword`s go into `find_in_words`.
  // Each `keyword` may have multiple terms, so loop over them.
  query_parser::QueryWordVector find_in_words;
  for (auto& keyword : cluster_keywords) {
    query_parser::QueryParser::ExtractQueryWords(base::i18n::ToLower(keyword),
                                                 &find_in_words);
  }

  return query_parser::QueryParser::DoesQueryMatch(find_in_words, find_nodes);
}

// Flags any elements within `cluster_visits` that match `find_nodes`. The
// matching is deliberately meant to closely mirror the History implementation.
// Returns the total score of matching visits, and returns 0 if no visits match.
float ComputeTotalMatchScore(
    const query_parser::QueryNodeVector& find_nodes,
    std::vector<history::ClusterVisit>* cluster_visits) {
  DCHECK(cluster_visits);

  float total_matching_visit_score = 0.0;

  for (auto& visit : *cluster_visits) {
    query_parser::QueryWordVector find_in_words;
    GURL gurl = visit.annotated_visit.url_row.url();

    std::u16string url_lower =
        base::i18n::ToLower(base::UTF8ToUTF16(gurl.possibly_invalid_spec()));
    query_parser::QueryParser::ExtractQueryWords(url_lower, &find_in_words);

    if (gurl.is_valid()) {
      // Decode punycode to match IDN.
      std::u16string ascii = base::ASCIIToUTF16(gurl.host());
      std::u16string utf = url_formatter::IDNToUnicode(gurl.host());
      if (ascii != utf)
        query_parser::QueryParser::ExtractQueryWords(utf, &find_in_words);
    }

    std::u16string title_lower =
        base::i18n::ToLower(visit.annotated_visit.url_row.title());
    query_parser::QueryParser::ExtractQueryWords(title_lower, &find_in_words);

    if (query_parser::QueryParser::DoesQueryMatch(find_in_words, find_nodes)) {
      visit.matches_search_query = true;
      DCHECK_GE(visit.score, 0);
      total_matching_visit_score += visit.score;
    }
  }

  return total_matching_visit_score;
}

// Re-scores and re-sorts `cluster_visits` so that all visits that match the
// search query are promoted above all visits that don't match the search query.
// All visits are likely to be rescored in this case.
//
// Note, this should NOT be called for `cluster_visits` with NO matching visits.
void PromoteMatchingVisitsAboveNonMatchingVisits(
    std::vector<history::ClusterVisit>* cluster_visits) {
  DCHECK(cluster_visits);
  for (auto& visit : *cluster_visits) {
    if (visit.matches_search_query) {
      // Smash all matching scores into the range that's above the fold.
      visit.score =
          GetConfig().min_score_to_always_show_above_the_fold +
          visit.score *
              (1 - GetConfig().min_score_to_always_show_above_the_fold);
    } else {
      // Smash all non-matching scores into the range that's below the fold.
      visit.score =
          visit.score * GetConfig().min_score_to_always_show_above_the_fold;
    }
  }

  StableSortVisits(cluster_visits);
}

}  // namespace

GURL ComputeURLForDeduping(const GURL& url) {
  // Below is a simplified version of `AutocompleteMatch::GURLToStrippedGURL`
  // that is thread-safe, stateless, never hits the disk, a bit more aggressive,
  // and without a dependency on omnibox components.
  if (!url.is_valid())
    return url;

  GURL url_for_deduping = url;

  GURL::Replacements replacements;

  // Strip out www, but preserve the eTLD+1. This matches the omnibox behavior.
  // Make an explicit local, as a StringPiece can't point to a temporary.
  std::string stripped_host = url_formatter::StripWWW(url_for_deduping.host());
  replacements.SetHostStr(base::StringPiece(stripped_host));

  // Replace http protocol with https. It's just for deduplication.
  if (url_for_deduping.SchemeIs(url::kHttpScheme))
    replacements.SetSchemeStr(url::kHttpsScheme);

  if (url.has_ref())
    replacements.ClearRef();

  url_for_deduping = url_for_deduping.ReplaceComponents(replacements);
  return url_for_deduping;
}

std::string ComputeURLKeywordForLookup(const GURL& url) {
  return history::VisitSegmentDatabase::ComputeSegmentName(
      ComputeURLForDeduping(url));
}

void StableSortVisits(std::vector<history::ClusterVisit>* visits) {
  DCHECK(visits);
  base::ranges::stable_sort(*visits, [](auto& v1, auto& v2) {
    if (v1.score != v2.score) {
      // Use v1 > v2 to get higher scored visits BEFORE lower scored visits.
      return v1.score > v2.score;
    }

    // Use v1 > v2 to get more recent visits BEFORE older visits.
    return v1.annotated_visit.visit_row.visit_time >
           v2.annotated_visit.visit_row.visit_time;
  });
}

void ApplySearchQuery(const std::string& query,
                      std::vector<history::Cluster>* clusters) {
  DCHECK(clusters);
  if (query.empty())
    return;

  // Extract query nodes from the query string.
  query_parser::QueryNodeVector find_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      base::UTF8ToUTF16(query),
      query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH, &find_nodes);

  // Move all the passed in `clusters` into `all_clusters`, and start rebuilding
  // `clusters` to only contain the matching ones.
  std::vector<history::Cluster> all_clusters;
  std::swap(all_clusters, *clusters);

  for (auto& cluster : all_clusters) {
    const float total_matching_visit_score =
        ComputeTotalMatchScore(find_nodes, &cluster.visits);
    DCHECK_GE(total_matching_visit_score, 0);
    if (total_matching_visit_score > 0 &&
        GetConfig().rescore_visits_within_clusters_for_query) {
      PromoteMatchingVisitsAboveNonMatchingVisits(&cluster.visits);
    }

    cluster.search_match_score = total_matching_visit_score;
    if (DoesQueryMatchClusterKeywords(find_nodes, cluster.keywords)) {
      // Arbitrarily chosen that cluster keyword matches are worth three points.
      // TODO(crbug.com/1307071): Use relevancy score for each cluster keyword
      // once support for that is added to the backend.
      cluster.search_match_score += 3.0;
    }

    if (cluster.search_match_score > 0) {
      // Move the matching clusters into the final list.
      clusters->push_back(std::move(cluster));
    }
  }

  if (GetConfig().sort_clusters_within_batch_for_query) {
    base::ranges::stable_sort(*clusters, [](auto& c1, auto& c2) {
      // Use c1 > c2 to get higher scored clusters BEFORE lower scored clusters.
      return c1.search_match_score > c2.search_match_score;
    });
  }
}

void CullNonProminentOrDuplicateClusters(
    std::string query,
    std::vector<history::Cluster>* clusters,
    std::set<GURL>* seen_single_visit_cluster_urls) {
  DCHECK(clusters);
  DCHECK(seen_single_visit_cluster_urls);
  if (query.empty()) {
    // For the empty-query state, only show clusters with
    // `should_show_on_prominent_ui_surfaces` set to true. This restriction is
    // NOT applied when the user is searching for a specific keyword.
    clusters->erase(base::ranges::remove_if(
                        *clusters,
                        [](const history::Cluster& cluster) {
                          return !cluster.should_show_on_prominent_ui_surfaces;
                        }),
                    clusters->end());
  } else {
    clusters->erase(base::ranges::remove_if(
                        *clusters,
                        [&](const history::Cluster& cluster) {
                          // Erase all duplicate single-visit non-prominent
                          // clusters.
                          if (!cluster.should_show_on_prominent_ui_surfaces &&
                              cluster.visits.size() == 1) {
                            auto [unused_iterator, newly_inserted] =
                                seen_single_visit_cluster_urls->insert(
                                    cluster.visits[0].url_for_deduping);
                            return !newly_inserted;
                          }

                          return false;
                        }),
                    clusters->end());
  }
}

}  // namespace history_clusters
