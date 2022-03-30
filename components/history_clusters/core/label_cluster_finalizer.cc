// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/label_cluster_finalizer.h"

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace history_clusters {

LabelClusterFinalizer::LabelClusterFinalizer() = default;
LabelClusterFinalizer::~LabelClusterFinalizer() = default;

void LabelClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  float max_label_score = -1;
  absl::optional<std::u16string> current_highest_scoring_label;

  // First try finding search terms to use as the cluster label.
  for (const auto& visit : cluster.visits) {
    if (!visit.annotated_visit.content_annotations.search_terms.empty() &&
        visit.score > max_label_score) {
      current_highest_scoring_label = l10n_util::GetStringFUTF16(
          IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS,
          visit.annotated_visit.content_annotations.search_terms);
      max_label_score = visit.score;
    }
  }

  // If we haven't found a label yet, use the Entity name, if that flag is
  // enabled.
  if (GetConfig().labels_from_entities && !current_highest_scoring_label) {
    base::flat_map<std::string, float> entity_to_score;
    for (const auto& visit : cluster.visits) {
      for (const auto& entity : visit.annotated_visit.content_annotations
                                    .model_annotations.entities) {
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
  }

  if (current_highest_scoring_label) {
    cluster.label = *current_highest_scoring_label;
  }
}

}  // namespace history_clusters
