// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_TEST_UTILS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_TEST_UTILS_H_

#include <vector>

#include "components/history/core/browser/history_types.h"

namespace history_clusters {
namespace testing {

// A helper object that contains the elements to validate that
// the result visits of clusters are correct.
class VisitResult {
 public:
  VisitResult(int visit_id,
              float score,
              const std::vector<history::VisitID>& duplicate_visit_ids = {},
              bool is_search_visit = false);
  explicit VisitResult(const history::ClusterVisit& visit);
  VisitResult(const VisitResult& other);
  ~VisitResult();

  bool operator==(const VisitResult& rhs) const;

 private:
  const int visit_id_;
  const float score_;
  const std::vector<history::VisitID> duplicate_visit_ids_;
  const bool is_search_visit_;
};

// Converts |clusters| to VisitResults which are easier to test equality for.
std::vector<std::vector<testing::VisitResult>> ToVisitResults(
    const std::vector<history::Cluster>& clusters);

// Creates a default AnnotatedVisit that has the minimal set of fields required.
history::AnnotatedVisit CreateDefaultAnnotatedVisit(int visit_id,
                                                    const GURL& url);

// Creates a ClusterVisit from |annotated_visit|. Will populate the returned
// ClusterVisit's normalized_url with |normalized_url| if present but otherwise
// will use the URL contained in the AnnotatedVisit.
history::ClusterVisit CreateClusterVisit(
    const history::AnnotatedVisit& annotated_visit,
    absl::optional<GURL> normalized_url = absl::nullopt);

}  // namespace testing
}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_TEST_UTILS_H_