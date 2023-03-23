// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/label_cluster_finalizer.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"

using LabelSource = history::Cluster::LabelSource;

namespace history_clusters {

LabelClusterFinalizer::LabelClusterFinalizer(
    base::flat_map<std::string, optimization_guide::EntityMetadata>*
        entity_metadata_map)
    : entity_metadata_map_(*entity_metadata_map) {}
LabelClusterFinalizer::~LabelClusterFinalizer() = default;

void LabelClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  float max_label_score = -1;
  absl::optional<std::u16string> current_highest_scoring_label;
  absl::optional<std::u16string> current_highest_scoring_label_unquoted;

  // First try finding search terms to use as the cluster label.
  int num_search_visits = 0;
  for (const auto& visit : cluster.visits) {
    if (!visit.annotated_visit.content_annotations.search_terms.empty()) {
      num_search_visits++;

      if (visit.score > max_label_score) {
        current_highest_scoring_label_unquoted =
            visit.annotated_visit.content_annotations.search_terms;
        current_highest_scoring_label = l10n_util::GetStringFUTF16(
            IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS,
            *current_highest_scoring_label_unquoted);
        max_label_score = visit.score;
      }
    }
  }

  // If there are multiple search visits, find the most common entity.
  if (GetConfig().labels_from_search_visit_entities && num_search_visits > 1) {
    base::flat_map<std::string, float> entity_to_score;
    base::flat_map<std::string, int> entity_to_count;
    for (const auto& visit : cluster.visits) {
      if (visit.annotated_visit.content_annotations.search_terms.empty()) {
        continue;
      }
      for (const auto& entity : visit.annotated_visit.content_annotations
                                    .model_annotations.entities) {
        auto it = entity_to_score.find(entity.id);
        float new_score = it != entity_to_score.end()
                              ? it->second + (entity.weight * visit.score)
                              : entity.weight * visit.score;
        entity_to_score[entity.id] = new_score;

        entity_to_count[entity.id]++;
      }
    }

    // Get entity most common amongst the search visits and tiebreak using
    // score.
    int max_count = -1;
    max_label_score = -1;
    absl::optional<std::string> highest_scoring_entity;
    for (const auto& entity_and_count : entity_to_count) {
      auto entity_metadata_it =
          entity_metadata_map_->find(entity_and_count.first);
      if (entity_metadata_it == entity_metadata_map_->end()) {
        continue;
      }

      if (entity_and_count.second > max_count) {
        max_count = entity_and_count.second;
        max_label_score = entity_to_score.at(entity_and_count.first);
        highest_scoring_entity = entity_metadata_it->second.human_readable_name;
      } else if (entity_and_count.second == max_count &&
                 entity_to_score.at(entity_and_count.first) > max_label_score) {
        max_label_score = entity_to_score.at(entity_and_count.first);
        highest_scoring_entity = entity_metadata_it->second.human_readable_name;
      }
    }

    if (highest_scoring_entity) {
      current_highest_scoring_label_unquoted =
          base::UTF8ToUTF16(*highest_scoring_entity);
      current_highest_scoring_label = l10n_util::GetStringFUTF16(
          IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS,
          *current_highest_scoring_label_unquoted);
    }
  }

  // If we haven't found a label yet, use Entities, if that flag is enabled.
  // TODO(crbug.com/1294348): Implement a configurable quality threshold, so
  // low quality Entity labels can be ignored in favor of hostnames below.
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
          auto entity_metadata_it = entity_metadata_map_->find(entity.id);
          if (entity_metadata_it == entity_metadata_map_->end()) {
            continue;
          }
          max_label_score = new_score;
          current_highest_scoring_label =
              base::UTF8ToUTF16(entity_metadata_it->second.human_readable_name);
          current_highest_scoring_label_unquoted =
              current_highest_scoring_label;
        }
        entity_to_score[entity.id] = new_score;
      }
    }
  }

  // If we haven't found a label yet, use hostnames if the flag is enabled.
  if (GetConfig().labels_from_hostnames && !current_highest_scoring_label) {
    base::flat_map<std::u16string, float> hostname_to_score;
    for (const auto& visit : cluster.visits) {
      std::u16string host =
          ComputeURLForDisplay(visit.normalized_url, /*trim_after_host=*/true);
      float& hostname_score = hostname_to_score[host];
      hostname_score += visit.score;
      if (hostname_score > max_label_score) {
        current_highest_scoring_label = host;
        current_highest_scoring_label_unquoted = current_highest_scoring_label;
        max_label_score = hostname_score;
      }
    }

    // At the end of this process, if we assigned a hostname label, yet there
    // is more than one hostname available, append " and more" to the label.
    if (current_highest_scoring_label && hostname_to_score.size() > 1) {
      current_highest_scoring_label = l10n_util::GetStringFUTF16(
          IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_MULTIPLE_HOSTNAMES,
          *current_highest_scoring_label);
    }
  }

  if (current_highest_scoring_label) {
    cluster.label = *current_highest_scoring_label;
    cluster.raw_label = *current_highest_scoring_label_unquoted;
  }
}

}  // namespace history_clusters
