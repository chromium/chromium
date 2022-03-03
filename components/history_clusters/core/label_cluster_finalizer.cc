// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/label_cluster_finalizer.h"

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

LabelClusterFinalizer::LabelClusterFinalizer() = default;
LabelClusterFinalizer::~LabelClusterFinalizer() = default;

void LabelClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::flat_map<std::string, float> entity_to_score;
  float max_label_score = -1;
  absl::optional<std::u16string> current_highest_scoring_label;

  for (const auto& visit : cluster.visits) {
    if (!visit.annotated_visit.content_annotations.search_terms.empty()) {
      // We have a search term, so use that instead. Clear out any old state.
      if (!entity_to_score.empty()) {
        entity_to_score.clear();
        max_label_score = -1;
        current_highest_scoring_label = absl::nullopt;
      }

      // Update with the highest scoring search term, if available.
      if (visit.score > max_label_score) {
        current_highest_scoring_label =
            visit.annotated_visit.content_annotations.search_terms;
        max_label_score = visit.score;
      }
      continue;
    }

    if (current_highest_scoring_label && entity_to_score.empty()) {
      // If we are here, we are tracking high-scoring search terms for the
      // cluster instead of falling back to entities.
      continue;
    }

    for (const auto& entity :
         visit.annotated_visit.content_annotations.model_annotations.entities) {
      auto it = entity_to_score.find(entity.id);
      float new_score = it != entity_to_score.end()
                            ? it->second + (entity.weight * visit.score)
                            : entity.weight * visit.score;
      if (new_score > max_label_score) {
        max_label_score = new_score;
        current_highest_scoring_label = base::UTF8ToUTF16(entity.id);
      }
      entity_to_score[entity.id] = new_score;
    }
  }

  // If we get here, the label is either the search terms with the highest visit
  // score or the entity with the highest score.
  if (current_highest_scoring_label) {
    cluster.label = *current_highest_scoring_label;
  }
}

}  // namespace history_clusters
