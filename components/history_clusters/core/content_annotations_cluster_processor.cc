// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/content_annotations_cluster_processor.h"

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"

namespace history_clusters {

namespace {

// Populates |entities_bows| and |categories_bows| from cluster index to the set
// of unique entities and categories, respectively, from each visit, a
// bag-of-words for each cluster.
void CreateBoWsForClusters(
    const std::vector<history::Cluster>& clusters,
    base::flat_map<int, base::flat_set<std::u16string>>* entities_bows,
    base::flat_map<int, base::flat_set<std::u16string>>* categories_bows) {
  // Create the BoWs for each cluster from the individual clusters.
  for (size_t cluster_idx = 0; cluster_idx < clusters.size(); cluster_idx++) {
    auto& cluster = clusters.at(cluster_idx);
    base::flat_set<std::u16string> entity_bag_of_words;
    base::flat_set<std::u16string> category_bag_of_words;
    for (const auto& visit : cluster.visits) {
      for (const auto& entity : visit.annotated_visit.content_annotations
                                    .model_annotations.entities) {
        entity_bag_of_words.insert(base::UTF8ToUTF16(entity.id));
      }
      for (const auto& category : visit.annotated_visit.content_annotations
                                      .model_annotations.categories) {
        category_bag_of_words.insert(base::UTF8ToUTF16(category.id));
      }
    }
    entities_bows->insert({cluster_idx, entity_bag_of_words});
    categories_bows->insert({cluster_idx, category_bag_of_words});
  }
}

// Return the Jaccard Similarity between two sets of
// strings.
float CalculateJaccardSimilarity(
    const base::flat_set<std::u16string>& cluster1,
    const base::flat_set<std::u16string>& cluster2) {
  // If both clusters are empty, we don't know if they're the same so just say
  // they're completely different.
  if (cluster1.empty() && cluster2.empty())
    return 0.0;

  base::flat_set<std::u16string> cluster_union;
  int intersection_size = 0;
  for (const auto& token : cluster1) {
    if (cluster2.find(token) != cluster2.end()) {
      intersection_size++;
    }
    cluster_union.insert(token);
  }
  cluster_union.insert(cluster2.begin(), cluster2.end());

  return cluster_union.empty()
             ? 0.0
             : intersection_size / (1.0 * cluster_union.size());
}

// Calculates the similarity of two clusters using an intersection similarity.
// Returns 1 if the clusters share more than a threshold number of tokens in
// common and 0 otherwise.
float CalculateIntersectionSimilarity(
    const base::flat_set<std::u16string>& cluster1,
    const base::flat_set<std::u16string>& cluster2) {
  // If both clusters are empty, we don't know if they're the same so just say
  // they're completely different.
  if (cluster1.empty() && cluster2.empty())
    return 0.0;

  int intersection_size = 0;
  for (const auto& token : cluster1) {
    if (cluster2.find(token) != cluster2.end()) {
      intersection_size++;
    }
  }
  return intersection_size >= GetConfig().cluster_interaction_threshold ? 1.0
                                                                        : 0.0;
}

// Returns the similarity score based on the configured similarity metric.
float CalculateSimilarityScore(const base::flat_set<std::u16string>& cluster1,
                               const base::flat_set<std::u16string>& cluster2) {
  if (GetConfig().content_cluster_on_intersection_similarity)
    return CalculateIntersectionSimilarity(cluster1, cluster2);
  return CalculateJaccardSimilarity(cluster1, cluster2);
}

// Returns whether two clusters should be merged together based on their
// |entity_similarity| and |category_similarity|. Both |entity_similarity| and
// |category_similarity| are expected to be between 0 and 1, inclusive.
bool ShouldMergeClusters(float entity_similarity, float category_similarity) {
  float max_score = GetConfig().content_clustering_entity_similarity_weight +
                    GetConfig().content_clustering_category_similarity_weight;
  if (max_score == 0)
    return 0.0;

  float cluster_similarity_score =
      (GetConfig().content_clustering_entity_similarity_weight *
           entity_similarity +
       GetConfig().content_clustering_category_similarity_weight *
           category_similarity) /
      max_score;
  float normalized_similarity_score =
      cluster_similarity_score >
      GetConfig().content_clustering_similarity_threshold;
  DCHECK(normalized_similarity_score >= 0 && normalized_similarity_score <= 1);
  return normalized_similarity_score;
}

}  // namespace

ContentAnnotationsClusterProcessor::ContentAnnotationsClusterProcessor() =
    default;
ContentAnnotationsClusterProcessor::~ContentAnnotationsClusterProcessor() =
    default;

std::vector<history::Cluster>
ContentAnnotationsClusterProcessor::ProcessClusters(
    const std::vector<history::Cluster>& clusters) {
  base::flat_map<int, base::flat_set<std::u16string>>
      cluster_idx_to_entity_bows;
  base::flat_map<int, base::flat_set<std::u16string>>
      cluster_idx_to_category_bows;
  CreateBoWsForClusters(clusters, &cluster_idx_to_entity_bows,
                        &cluster_idx_to_category_bows);

  // Now cluster on the entries in each BoW between clusters.
  std::vector<history::Cluster> aggregated_clusters;
  base::flat_set<int> merged_cluster_indices;
  for (size_t i = 0; i < clusters.size(); i++) {
    if (merged_cluster_indices.find(i) != merged_cluster_indices.end()) {
      continue;
    }
    // Greedily combine clusters by checking if this cluster is similar to any
    // other unmerged clusters.
    history::Cluster aggregated_cluster = clusters[i];
    for (size_t j = i + 1; j < clusters.size(); j++) {
      if (merged_cluster_indices.find(j) != merged_cluster_indices.end()) {
        continue;
      }
      float entity_similarity = CalculateSimilarityScore(
          cluster_idx_to_entity_bows[i], cluster_idx_to_entity_bows[j]);
      float category_similarity = CalculateSimilarityScore(
          cluster_idx_to_category_bows[i], cluster_idx_to_category_bows[j]);
      if (ShouldMergeClusters(entity_similarity, category_similarity)) {
        // Add the visits to the aggregated cluster.
        merged_cluster_indices.insert(j);
        aggregated_cluster.visits.insert(aggregated_cluster.visits.end(),
                                         clusters[j].visits.begin(),
                                         clusters[j].visits.end());
      }
    }
    aggregated_clusters.push_back(std::move(aggregated_cluster));
  }
  return aggregated_clusters;
}

}  // namespace history_clusters
