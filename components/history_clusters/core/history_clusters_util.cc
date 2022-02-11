// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_util.h"

#include "base/i18n/case_conversion.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/query_parser/query_parser.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

// Returns true if `find_nodes` matches `cluster`.
// This is deliberately meant to closely mirror the History implementation..
// TODO(tommycli): Merge with `URLDatabase::GetTextMatchesWithAlgorithm()`.
bool DoesQueryMatchCluster(const query_parser::QueryNodeVector& find_nodes,
                           const history::Cluster& cluster) {
  query_parser::QueryWordVector find_in_words;

  // All of the cluster's `keyword`s go into `find_in_words`.
  // Each `keyword` may have multiple terms, so loop over them.
  for (auto& keyword : cluster.keywords) {
    query_parser::QueryParser::ExtractQueryWords(base::i18n::ToLower(keyword),
                                                 &find_in_words);
  }

  // Also extract all of the visits' URLs and titles into `find_in_words`.
  for (const auto& visit : cluster.visits) {
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
  }

  return query_parser::QueryParser::DoesQueryMatch(find_in_words, find_nodes);
}

}  // namespace

std::vector<history::Cluster> FilterClustersMatchingQuery(
    std::string query,
    std::vector<history::Cluster> clusters) {
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
    return clusters;
  }

  // Extract query nodes from the query string.
  query_parser::QueryNodeVector find_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      base::UTF8ToUTF16(query),
      query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH, &find_nodes);

  clusters.erase(base::ranges::remove_if(
                     clusters,
                     [&find_nodes](const history::Cluster& cluster) {
                       return !DoesQueryMatchCluster(find_nodes, cluster);
                     }),
                 clusters.end());
  return clusters;
}

}  // namespace history_clusters
