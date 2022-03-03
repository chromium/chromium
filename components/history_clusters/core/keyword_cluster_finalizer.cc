// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/keyword_cluster_finalizer.h"

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"

namespace history_clusters {

KeywordClusterFinalizer::KeywordClusterFinalizer() = default;
KeywordClusterFinalizer::~KeywordClusterFinalizer() = default;

void KeywordClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::flat_set<std::u16string> keywords_set;
  for (const auto& visit : cluster.visits) {
    if (GetConfig().should_exclude_keywords_from_noisy_visits &&
        IsNoisyVisit(visit)) {
      // Do not put keywords if user visits the page a lot and it's not a
      // search-like visit.
      continue;
    }

    for (const auto& entity :
         visit.annotated_visit.content_annotations.model_annotations.entities) {
      keywords_set.insert(base::UTF8ToUTF16(entity.id));
    }
    if (GetConfig().should_include_categories_in_keywords) {
      for (const auto& category : visit.annotated_visit.content_annotations
                                      .model_annotations.categories) {
        keywords_set.insert(base::UTF8ToUTF16(category.id));
      }
    }
  }
  cluster.keywords =
      std::vector<std::u16string>(keywords_set.begin(), keywords_set.end());
}

}  // namespace history_clusters
