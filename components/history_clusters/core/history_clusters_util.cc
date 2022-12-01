// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_util.h"

#include <algorithm>

#include "base/containers/contains.h"
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
#include "components/query_parser/snippet.h"
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
float MarkMatchesAndGetScore(const query_parser::QueryNodeVector& find_nodes,
                             history::Cluster* cluster) {
  DCHECK(cluster);
  float total_matching_visit_score = 0.0;

  if (cluster->label &&
      query_parser::QueryParser::DoesQueryMatch(
          *(cluster->label), find_nodes, &(cluster->label_match_positions))) {
    total_matching_visit_score += 1.0;
  }

  for (auto& visit : cluster->visits) {
    bool match_found = false;

    // Search through the visible elements and highlight match positions.
    GURL gurl = visit.annotated_visit.url_row.url();
    auto url_for_display_lower = base::i18n::ToLower(visit.url_for_display);
    match_found |= query_parser::QueryParser::DoesQueryMatch(
        url_for_display_lower, find_nodes,
        &visit.url_for_display_match_positions);
    auto title_lower =
        base::i18n::ToLower(visit.annotated_visit.url_row.title());
    match_found |= query_parser::QueryParser::DoesQueryMatch(
        title_lower, find_nodes, &visit.title_match_positions);

    // If we couldn't find it in the visible elements, try a second search
    // where we put all the text into one bag and try again. We need to do this
    // to be as exhaustive as History.
    // TODO(tommycli): Downgrade the score of these matches, or investigate if
    // we can discard this second pass. The consequence of discarding the second
    // pass is to omit matches that span both the URL and title.
    if (!match_found) {
      query_parser::QueryWordVector find_in_words;
      std::u16string url_lower =
          base::i18n::ToLower(base::UTF8ToUTF16(gurl.possibly_invalid_spec()));
      query_parser::QueryParser::ExtractQueryWords(url_lower, &find_in_words);
      query_parser::QueryParser::ExtractQueryWords(url_for_display_lower,
                                                   &find_in_words);
      query_parser::QueryParser::ExtractQueryWords(title_lower, &find_in_words);

      match_found |=
          query_parser::QueryParser::DoesQueryMatch(find_in_words, find_nodes);
    }

    if (match_found) {
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
    std::vector<history::ClusterVisit>& cluster_visits) {
  for (auto& visit : cluster_visits) {
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
  // that is thread-safe, stateless, never hits the disk, a lot more aggressive,
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

  // It's unusual to clear the query for deduping, because it's normally
  // considered a meaningful part of the URL. However, the return value of this
  // function isn't used naively. Details at `ClusterVisit::url_for_deduping`.
  if (url.has_query())
    replacements.ClearQuery();

  if (url.has_ref())
    replacements.ClearRef();

  if (GetConfig().use_host_for_visit_deduping && url.has_path()) {
    replacements.ClearPath();
  }

  url_for_deduping = url_for_deduping.ReplaceComponents(replacements);
  return url_for_deduping;
}

std::string ComputeURLKeywordForLookup(const GURL& url) {
  return history::VisitSegmentDatabase::ComputeSegmentName(
      ComputeURLForDeduping(url));
}

std::u16string ComputeURLForDisplay(const GURL& url, bool trim_after_host) {
  // Use URL formatting options similar to the omnibox popup. The url_formatter
  // component does IDN hostname conversion as well.
  url_formatter::FormatUrlTypes format_types =
      url_formatter::kFormatUrlOmitDefaults |
      url_formatter::kFormatUrlOmitHTTPS |
      url_formatter::kFormatUrlOmitTrivialSubdomains;

  if (trim_after_host)
    format_types |= url_formatter::kFormatUrlTrimAfterHost;

  return url_formatter::FormatUrl(url, format_types, base::UnescapeRule::SPACES,
                                  nullptr, nullptr, nullptr);
}

void StableSortVisits(std::vector<history::ClusterVisit>& visits) {
  base::ranges::stable_sort(visits, [](auto& v1, auto& v2) {
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
                      std::vector<history::Cluster>& clusters) {
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
  std::swap(all_clusters, clusters);

  for (auto& cluster : all_clusters) {
    const float total_matching_visit_score =
        MarkMatchesAndGetScore(find_nodes, &cluster);
    DCHECK_GE(total_matching_visit_score, 0);
    if (total_matching_visit_score > 0 &&
        GetConfig().rescore_visits_within_clusters_for_query) {
      PromoteMatchingVisitsAboveNonMatchingVisits(cluster.visits);
    }

    cluster.search_match_score = total_matching_visit_score;

    if (DoesQueryMatchClusterKeywords(find_nodes, cluster.GetKeywords())) {
      // Arbitrarily chosen that cluster keyword matches are worth three points.
      // TODO(crbug.com/1307071): Use relevancy score for each cluster keyword
      // once support for that is added to the backend.
      cluster.search_match_score += 3.0;
    }

    if (cluster.search_match_score > 0) {
      // Move the matching clusters into the final list.
      clusters.push_back(std::move(cluster));
    }
  }

  if (GetConfig().sort_clusters_within_batch_for_query) {
    base::ranges::stable_sort(clusters, [](auto& c1, auto& c2) {
      // Use c1 > c2 to get higher scored clusters BEFORE lower scored clusters.
      return c1.search_match_score > c2.search_match_score;
    });
  }
}

void CullNonProminentOrDuplicateClusters(
    std::string query,
    std::vector<history::Cluster>& clusters,
    std::set<GURL>* seen_single_visit_cluster_urls) {
  DCHECK(seen_single_visit_cluster_urls);
  if (GetConfig()
          .should_show_all_clusters_unconditionally_on_prominent_ui_surfaces) {
    // Do not cull if we should just show everything.
    return;
  }

  if (query.empty()) {
    // For the empty-query state, only show clusters with
    // `should_show_on_prominent_ui_surfaces` set to true. This restriction is
    // NOT applied when the user is searching for a specific keyword.
    clusters.erase(base::ranges::remove_if(
                       clusters,
                       [](const history::Cluster& cluster) {
                         return !cluster.should_show_on_prominent_ui_surfaces;
                       }),
                   clusters.end());
  } else {
    clusters.erase(base::ranges::remove_if(
                       clusters,
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
                   clusters.end());
  }
}

void HideAndCullLowScoringVisits(std::vector<history::Cluster>& clusters) {
  for (auto& cluster : clusters) {
    for (size_t i = 0; i < cluster.visits.size(); ++i) {
      auto& visit = cluster.visits[i];
      // Even a 0.0 visit shouldn't be hidden if this is the first visit we
      // encounter. The assumption is that the visits are always ranked by score
      // in a descending order.
      // TODO(crbug.com/1313631): Simplify this after removing "Show More" UI.
      if ((visit.score == 0.0 && i != 0) ||
          (visit.score < GetConfig().min_score_to_always_show_above_the_fold &&
           i >= GetConfig().num_visits_to_always_show_above_the_fold)) {
        visit.hidden = true;
      }
    }

    if (GetConfig().drop_hidden_visits) {
      cluster.visits.erase(
          base::ranges::remove_if(
              cluster.visits, [](const auto& visit) { return visit.hidden; }),
          cluster.visits.end());
    }
  }
}

void CoalesceRelatedSearches(std::vector<history::Cluster>& clusters) {
  constexpr size_t kMaxRelatedSearches = 5;

  for (auto& cluster : clusters) {
    for (const auto& visit : cluster.visits) {
      // Coalesce the unique related searches of this visit into the cluster
      // until the cap is reached.
      for (const auto& search_query :
           visit.annotated_visit.content_annotations.related_searches) {
        if (cluster.related_searches.size() >= kMaxRelatedSearches) {
          return;
        }

        if (!base::Contains(cluster.related_searches, search_query)) {
          cluster.related_searches.push_back(search_query);
        }
      }
    }
  }
}

void SortClusters(std::vector<history::Cluster>* clusters) {
  DCHECK(clusters);
  // Within each cluster, sort visits.
  for (auto& cluster : *clusters) {
    StableSortVisits(cluster.visits);
  }

  // After that, sort clusters reverse-chronologically based on their highest
  // scored visit.
  base::ranges::stable_sort(*clusters, [&](auto& c1, auto& c2) {
    if (c1.visits.empty()) {
      return false;
    }
    if (c2.visits.empty()) {
      return true;
    }

    base::Time c1_time = c1.visits.front().annotated_visit.visit_row.visit_time;
    base::Time c2_time = c2.visits.front().annotated_visit.visit_row.visit_time;

    // Use c1 > c2 to get more recent clusters BEFORE older clusters.
    return c1_time > c2_time;
  });
}

}  // namespace history_clusters
