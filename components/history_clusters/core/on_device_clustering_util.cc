// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/on_device_clustering_util.h"

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/core/on_device_clustering_features.h"

namespace history_clusters {

void MergeDuplicateVisitIntoCanonicalVisit(
    history::ClusterVisit&& duplicate_visit,
    history::ClusterVisit& canonical_visit) {
  // Upgrade the canonical visit's annotations (i.e. is-bookmarked) with
  // those of the duplicate visits.
  auto& context_annotations =
      canonical_visit.annotated_visit.context_annotations;
  const auto& duplicate_annotations =
      duplicate_visit.annotated_visit.context_annotations;
  context_annotations.is_existing_bookmark |=
      duplicate_annotations.is_existing_bookmark;
  context_annotations.is_existing_part_of_tab_group |=
      duplicate_annotations.is_existing_part_of_tab_group;
  context_annotations.is_new_bookmark |= duplicate_annotations.is_new_bookmark;
  context_annotations.is_placed_in_tab_group |=
      duplicate_annotations.is_placed_in_tab_group;
  context_annotations.is_ntp_custom_link |=
      duplicate_annotations.is_ntp_custom_link;
  context_annotations.omnibox_url_copied |=
      duplicate_annotations.omnibox_url_copied;

  auto& canonical_searches =
      canonical_visit.annotated_visit.content_annotations.related_searches;
  const auto& duplicate_searches =
      duplicate_visit.annotated_visit.content_annotations.related_searches;
  for (const auto& query : duplicate_searches) {
    // This is an n^2 algorithm, but in practice the list of related
    // searches should be on the order of 10 elements long at maximum.
    // If that's not true we should replace this with a set structure.
    if (!base::Contains(canonical_searches, query)) {
      canonical_searches.push_back(query);
    }
  }

  // Merge over the model annotations (categories and entities) too.
  canonical_visit.annotated_visit.content_annotations.model_annotations
      .MergeFrom(duplicate_visit.annotated_visit.content_annotations
                     .model_annotations);

  // Roll up the visit duration from the duplicate visit into the canonical
  // visit.
  canonical_visit.annotated_visit.visit_row.visit_duration +=
      duplicate_visit.annotated_visit.visit_row.visit_duration;
  // Only add the foreground duration if it is populated and roll it up.
  base::TimeDelta duplicate_foreground_duration =
      duplicate_visit.annotated_visit.context_annotations
          .total_foreground_duration;
  // Check for > 0 since the default for total_foreground_duration is -1.
  if (duplicate_foreground_duration > base::Seconds(0)) {
    base::TimeDelta canonical_foreground_duration =
        canonical_visit.annotated_visit.context_annotations
            .total_foreground_duration;
    canonical_visit.annotated_visit.context_annotations
        .total_foreground_duration =
        canonical_foreground_duration > base::Seconds(0)
            ? canonical_foreground_duration + duplicate_foreground_duration
            : duplicate_foreground_duration;
  }

  // Update the canonical_visit with the more recent timestamp.
  canonical_visit.annotated_visit.visit_row.visit_time =
      std::max(canonical_visit.annotated_visit.visit_row.visit_time,
               duplicate_visit.annotated_visit.visit_row.visit_time);

  canonical_visit.duplicate_visits.push_back(std::move(duplicate_visit));
}

void SortClusters(std::vector<history::Cluster>* clusters) {
  DCHECK(clusters);
  // Within each cluster, sort visits.
  for (auto& cluster : *clusters) {
    StableSortVisits(&cluster.visits);
  }

  // After that, sort clusters reverse-chronologically based on their highest
  // scored visit.
  base::ranges::stable_sort(*clusters, [&](auto& c1, auto& c2) {
    DCHECK(!c1.visits.empty());
    base::Time c1_time = c1.visits.front().annotated_visit.visit_row.visit_time;

    DCHECK(!c2.visits.empty());
    base::Time c2_time = c2.visits.front().annotated_visit.visit_row.visit_time;

    // Use c1 > c2 to get more recent clusters BEFORE older clusters.
    return c1_time > c2_time;
  });
}

bool IsNoisyVisit(const history::ClusterVisit& visit) {
  return visit.engagement_score >
             GetConfig().noisy_cluster_visits_engagement_threshold &&
         visit.annotated_visit.content_annotations.search_terms.empty();
}

}  // namespace history_clusters
