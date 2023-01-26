// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/content_annotations_cluster_processor.h"

#include <math.h>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
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

}  // namespace

ContentAnnotationsClusterProcessor::ContentAnnotationsClusterProcessor(
    base::flat_map<std::string, optimization_guide::EntityMetadata>*
        entity_id_to_entity_metadata_map)
    : entity_id_to_entity_metadata_map_(*entity_id_to_entity_metadata_map) {}
ContentAnnotationsClusterProcessor::~ContentAnnotationsClusterProcessor() =
    default;

void ContentAnnotationsClusterProcessor::ProcessClusters(
    std::vector<history::Cluster>* clusters) {
  std::vector<base::flat_map<std::string, float>> occurrence_maps(
      clusters->size());
  for (size_t i = 0; i < clusters->size(); i++) {
    occurrence_maps[i] = CreateOccurrenceMapForCluster(clusters->at(i));
  }

  // Now cluster on the entries in each BoW between clusters.
  base::flat_set<int> merged_cluster_indices;
  for (size_t i = 0; i < clusters->size(); i++) {
    if (merged_cluster_indices.find(i) != merged_cluster_indices.end()) {
      continue;
    }
    // Greedily combine clusters by checking if this cluster is similar to any
    // other unmerged clusters.
    for (size_t j = i + 1; j < clusters->size(); j++) {
      if (merged_cluster_indices.find(j) != merged_cluster_indices.end()) {
        continue;
      }
      float entity_similarity =
          CalculateSimilarityScore(occurrence_maps[i], occurrence_maps[j]);
      if (entity_similarity >
          GetConfig().content_clustering_similarity_threshold) {
        // Add the visits to the aggregated cluster.
        merged_cluster_indices.insert(j);
        AppendClusterVisits(clusters->at(i), clusters->at(j));
      }
    }
  }

  // Remove empty clusters.
  RemoveEmptyClusters(clusters);
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
