// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_SIMILAR_VISIT_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_SIMILAR_VISIT_H_

#include <string>

#include "base/hash/hash.h"
#include "components/history/core/browser/history_types.h"

namespace history_clusters {

// Represents a visit that can be used for deduping.
struct SimilarVisit {
  SimilarVisit() = default;
  explicit SimilarVisit(const history::ClusterVisit& visit)
      : title(visit.annotated_visit.url_row.title()),
        url_for_deduping(visit.url_for_deduping.spec()),
        normalized_url(visit.normalized_url.spec()) {}
  SimilarVisit(const SimilarVisit&) = default;
  ~SimilarVisit() = default;

  std::u16string title;
  std::string url_for_deduping;
  std::string normalized_url;

  struct Hash {
    size_t operator()(const SimilarVisit& visit) const {
      // Return the same hash for everything so it falls back to tiebreaking by
      // the Equals operator.
      return 0;
    }
  };

  struct Equals {
    bool operator()(const SimilarVisit& lhs, const SimilarVisit& rhs) const {
      if ((lhs.title == rhs.title) &&
          (lhs.url_for_deduping == rhs.url_for_deduping)) {
        return true;
      }
      return lhs.normalized_url == rhs.normalized_url;
    }
  };
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_SIMILAR_VISIT_H_
