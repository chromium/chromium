// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/keyword_cluster_finalizer.h"

#include <queue>

#include "base/containers/contains.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

namespace {

static constexpr float kSearchTermsScore = 100.0;
static constexpr float kScoreEpsilon = 1e-8;

void KeepTopKeywords(
    base::flat_map<std::u16string, history::ClusterKeywordData>&
        keyword_to_data_map,
    size_t max_num_keywords_per_cluster) {
  if (keyword_to_data_map.size() <= max_num_keywords_per_cluster) {
    return;
  }

  // Compare keywords first by their scores and then by types.
  auto cmp_keywords =
      [](const std::pair<std::u16string, const history::ClusterKeywordData*>&
             left,
         const std::pair<std::u16string, const history::ClusterKeywordData*>&
             right) {
        return std::fabs(left.second->score - right.second->score) >
                       kScoreEpsilon
                   ? left.second->score > right.second->score
                   : left.second->type > right.second->type;
      };

  // Minimum priority queue of top keywords.
  std::priority_queue<
      std::pair<std::u16string, const history::ClusterKeywordData*>,
      std::vector<
          std::pair<std::u16string, const history::ClusterKeywordData*>>,
      decltype(cmp_keywords)>
      pq(cmp_keywords);
  for (const auto& keyword_data_p : keyword_to_data_map) {
    bool should_insert = false;
    if (pq.size() < max_num_keywords_per_cluster) {
      should_insert = true;
    } else {
      if (pq.top().second->score < keyword_data_p.second.score) {
        pq.pop();
        should_insert = true;
      }
    }
    if (should_insert) {
      pq.push(std::make_pair(keyword_data_p.first, &keyword_data_p.second));
    }
  }

  base::flat_set<std::u16string> keywords_set;
  while (!pq.empty()) {
    keywords_set.insert(pq.top().first);
    pq.pop();
  }

  auto it = keyword_to_data_map.begin();
  for (; it != keyword_to_data_map.end();) {
    if (!keywords_set.contains(it->first)) {
      it = keyword_to_data_map.erase(it);
    } else {
      it++;
    }
  }
  DCHECK_EQ(keyword_to_data_map.size(), max_num_keywords_per_cluster);
}

}  // namespace

KeywordClusterFinalizer::KeywordClusterFinalizer() = default;
KeywordClusterFinalizer::~KeywordClusterFinalizer() = default;

void KeywordClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::flat_map<std::u16string, history::ClusterKeywordData>
      keyword_to_data_map;
  for (const auto& visit : cluster.visits) {
    if (!GetConfig().keyword_filter_on_noisy_visits && IsNoisyVisit(visit)) {
      // Do not put keywords if user visits the page a lot and it's not a
      // search-like visit.
      continue;
    }

    if (!visit.annotated_visit.content_annotations.search_terms.empty()) {
      const auto& search_terms =
          visit.annotated_visit.content_annotations.search_terms;
      auto search_it = keyword_to_data_map.find(search_terms);
      if (search_it == keyword_to_data_map.end()) {
        keyword_to_data_map[search_terms] = history::ClusterKeywordData(
            history::ClusterKeywordData::kSearchTerms,
            /*score=*/kSearchTermsScore);
      } else {
        search_it->second.score += kSearchTermsScore;
        search_it->second.MaybeUpdateKeywordType(
            history::ClusterKeywordData::kSearchTerms);
      }
    }
  }

  KeepTopKeywords(keyword_to_data_map,
                  GetConfig().max_num_keywords_per_cluster);

  cluster.keyword_to_data_map = std::move(keyword_to_data_map);
}

}  // namespace history_clusters
