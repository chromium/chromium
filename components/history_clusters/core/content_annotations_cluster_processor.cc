// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/content_annotations_cluster_processor.h"

#include <math.h>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/optimization_guide/core/entity_metadata.h"

namespace history_clusters {

namespace {

float CalculateMagnitude(const base::flat_map<std::string, float>& cluster) {
  float magnitude = 0.0;
  for (const auto& token : cluster) {
    magnitude += token.second * token.second;
  }
  return sqrt(magnitude);
}

// Returns the cosine similarity between two occurrence maps.
float CalculateCosineSimiliarity(
    const base::flat_map<std::string, float>& cluster1,
    const base::flat_map<std::string, float>& cluster2) {
  // If either cluster is empty, just say that they are different.
  if (cluster1.empty() || cluster2.empty())
    return 0.0;

  base::flat_set<std::string> all_words;
  for (const auto& token : cluster1) {
    all_words.insert(token.first);
  }
  for (const auto& token : cluster2) {
    all_words.insert(token.first);
  }

  float dot_product = 0.0;
  for (const auto& word : all_words) {
    float cluster1_value = cluster1.contains(word) ? cluster1.at(word) : 0.0;
    float cluster2_value = cluster2.contains(word) ? cluster2.at(word) : 0.0;
    dot_product += cluster1_value * cluster2_value;
  }
  float mag_cluster1 = CalculateMagnitude(cluster1);
  float mag_cluster2 = CalculateMagnitude(cluster2);

  return dot_product / (mag_cluster1 * mag_cluster2);
}

// Returns the similarity score based on the configured similarity metric.
float CalculateSimilarityScore(
    const base::flat_map<std::string, float>& cluster1,
    const base::flat_map<std::string, float>& cluster2) {
  // TODO(b/244505276): Add more similarity metrics here if we can find one that
  // makes more sense than cosine similarity.
  return CalculateCosineSimiliarity(cluster1, cluster2);
}

// Merges based on the following algorithm:
// For each cluster in clusters:
//   For each other cluster in clusters:
//     If similar, merge into first cluster.
void MergeIntoPreviousClusters(
    std::vector<history::Cluster>* clusters,
    std::vector<base::flat_map<std::string, float>>& occurrence_maps) {
  for (size_t i = 0; i < clusters->size(); i++) {
    if (clusters->at(i).visits.empty()) {
      continue;
    }
    // Greedily combine clusters by checking if this cluster is similar to any
    // other unmerged clusters.
    for (size_t j = i + 1; j < clusters->size(); j++) {
      if (clusters->at(j).visits.empty()) {
        continue;
      }
      float entity_similarity =
          CalculateSimilarityScore(occurrence_maps[i], occurrence_maps[j]);
      if (entity_similarity >=
          GetConfig().content_clustering_similarity_threshold) {
        // Add the visits to the aggregated cluster.
        AppendClusterVisits(clusters->at(i), clusters->at(j));
      }
    }
  }
}

// Performs a pairwise merge of similar clusters in `clusters` based on
// `occurrence_maps`. In each iteration of the calculation, it will find the
// closest match for each cluster based on the similarity of the occurrence maps
// for each non-empty cluster in `clusters` and merge clusters that have a
// reciprocal match. It will run until the merge converges (an iteration has no
// matches) or the max number of iterations have been run.
void PairwiseMergeSimilarClusters(
    std::vector<history::Cluster>* clusters,
    std::vector<base::flat_map<std::string, float>>& occurrence_maps) {
  int num_iterations = 0;
  base::flat_set<size_t> no_matches;
  bool found_match_in_iteration = false;

  do {
    num_iterations++;
    found_match_in_iteration = false;

    base::flat_map<size_t, size_t> best_matches;
    base::flat_map<size_t, float> best_matches_scores;

    for (size_t i = 0; i < clusters->size(); i++) {
      if (no_matches.contains(i)) {
        // Skip if it did not have a match in a previous iteration.
        continue;
      }
      if (clusters->at(i).visits.empty()) {
        // Skip if cluster has already been merged.
        continue;
      }

      for (size_t j = i + 1; j < clusters->size(); j++) {
        if (no_matches.contains(j)) {
          // Skip if it did not have a match in a previous iteration.
          continue;
        }
        if (clusters->at(j).visits.empty()) {
          // Skip if cluster has already been merged.
          continue;
        }

        float entity_similarity =
            CalculateSimilarityScore(occurrence_maps[i], occurrence_maps[j]);
        if (entity_similarity >=
            GetConfig().content_clustering_similarity_threshold) {
          found_match_in_iteration = true;

          // Update best match for i, if applicable.
          if (!best_matches_scores.contains(i) ||
              entity_similarity > best_matches_scores[i]) {
            best_matches[i] = j;
            best_matches_scores[i] = entity_similarity;
          }

          // Update best match for j, if applicable.
          if (!best_matches_scores.contains(j) ||
              entity_similarity > best_matches_scores[j]) {
            best_matches[j] = i;
            best_matches_scores[j] = entity_similarity;
          }
        }
      }

      if (!best_matches.contains(i)) {
        // Did not find a match for `i` during this iteration. Keep track of it
        // so future processing of `i` is not performed.
        no_matches.insert(i);
      }
    }

    // Process potential matches.
    for (const auto& match : best_matches) {
      DCHECK(clusters->size() > match.first && clusters->size() > match.second);

      if (clusters->at(match.first).visits.empty()) {
        // Skip cluster match that has already been processed.
        continue;
      }

      // See if it is a reciprocal match.
      if (best_matches.contains(match.second) &&
          best_matches[match.second] == match.first) {
        // Merge cluster visits from second cluster into first.
        AppendClusterVisits(clusters->at(match.first),
                            clusters->at(match.second));

        // Merge occurrence mappings from second cluster into first.
        for (const auto& entity_and_occurrence :
             occurrence_maps[match.second]) {
          occurrence_maps[match.first][entity_and_occurrence.first] +=
              entity_and_occurrence.second;
        }
        occurrence_maps[match.second].clear();
      }
    }
  } while (found_match_in_iteration &&
           num_iterations < GetConfig().max_pairwise_merge_iterations);

  base::UmaHistogramCounts100(
      "History.Clusters.Backend.ContentClustering.PairwiseMergeNumIterations",
      num_iterations);
}

// Merges similar clusters in `clusters` based on the `occurrence_maps`. It is
// expected that `clusters` and `occurrence_maps` have the same number of
// entries and that the ith entry in `occurence_maps` is the occurrence map for
// the ith entry in `clusters`.
void MergeSimilarClusters(
    std::vector<history::Cluster>* clusters,
    std::vector<base::flat_map<std::string, float>>& occurrence_maps) {
  DCHECK_EQ(clusters->size(), occurrence_maps.size());

  if (GetConfig().use_pairwise_merge) {
    PairwiseMergeSimilarClusters(clusters, occurrence_maps);
  } else {
    MergeIntoPreviousClusters(clusters, occurrence_maps);
  }

  RemoveEmptyClusters(clusters);
}

}  // namespace

ContentAnnotationsClusterProcessor::ContentAnnotationsClusterProcessor(
    base::flat_map<std::string, optimization_guide::EntityMetadata>*
        entity_id_to_entity_metadata_map)
    : entity_id_to_entity_metadata_map_(*entity_id_to_entity_metadata_map) {}
ContentAnnotationsClusterProcessor::~ContentAnnotationsClusterProcessor() =
    default;

void ContentAnnotationsClusterProcessor::ProcessClusters(
    std::vector<history::Cluster>* clusters) {
  DCHECK(clusters);

  if (clusters->empty()) {
    return;
  }

  base::UmaHistogramCounts1000(
      "History.Clusters.Backend.ContentClustering.NumClustersBeforeMerge",
      clusters->size());

  std::vector<base::flat_map<std::string, float>> occurrence_maps(
      clusters->size());
  for (size_t i = 0; i < clusters->size(); i++) {
    occurrence_maps[i] = CreateOccurrenceMapForCluster(clusters->at(i));
  }

  MergeSimilarClusters(clusters, occurrence_maps);

  base::UmaHistogramCounts1000(
      "History.Clusters.Backend.ContentClustering.NumClustersAfterMerge",
      clusters->size());
}

base::flat_map<std::string, float>
ContentAnnotationsClusterProcessor::CreateOccurrenceMapForCluster(
    const history::Cluster& cluster) {
  base::flat_map<std::string, float> occurrence_map;
  for (const auto& visit : cluster.visits) {
    for (const auto& entity :
         visit.annotated_visit.content_annotations.model_annotations.entities) {
      auto entity_metadata_it =
          entity_id_to_entity_metadata_map_->find(entity.id);
      if (entity_metadata_it == entity_id_to_entity_metadata_map_->end()) {
        continue;
      }
      auto& entity_metadata = entity_metadata_it->second;

      // Check whether the entity has any collections.
      if (GetConfig()
              .exclude_entities_that_have_no_collections_from_content_clustering &&
          entity_metadata.collections.empty()) {
        continue;
      }

      // Check whether any of the tagged collections are part of the collection
      // blocklist.
      if (!GetConfig().collections_to_block_from_content_clustering.empty()) {
        bool has_blocklisted_collection = false;
        for (const auto& collection : entity_metadata.collections) {
          if (GetConfig().collections_to_block_from_content_clustering.contains(
                  collection)) {
            has_blocklisted_collection = true;
            break;
          }
        }
        if (has_blocklisted_collection) {
          continue;
        }
      }

      occurrence_map[entity.id] += 1.0;
    }
  }
  return occurrence_map;
}

}  // namespace history_clusters
