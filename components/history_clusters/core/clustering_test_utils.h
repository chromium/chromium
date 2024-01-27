// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_TEST_UTILS_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_TEST_UTILS_H_

#include <cstdint>
#include <ostream>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"

namespace history_clusters::testing {

// A helper object that contains the elements to validate that
// the result visits of clusters are correct.
class VisitResult {
 public:
  VisitResult(
      int visit_id,
      float score,
      const std::vector<history::DuplicateClusterVisit>& duplicate_visits = {},
      std::u16string search_terms = u"");
  explicit VisitResult(const history::ClusterVisit& visit);
  VisitResult(const VisitResult& other);
  ~VisitResult();

  bool operator==(const VisitResult& rhs) const;

  std::string ToString() const;

 private:
  friend std::ostream& operator<<(std::ostream& os, const VisitResult& dt);

  const int visit_id_;
  const float score_;
  std::vector<history::DuplicateClusterVisit> duplicate_visits_;
  const std::u16string search_terms_;
};

std::ostream& operator<<(std::ostream& os, const VisitResult& dt);

// Converts |clusters| to VisitResults which are easier to test equality for.
std::vector<std::vector<testing::VisitResult>> ToVisitResults(
    const std::vector<history::Cluster>& clusters);

// Creates a default AnnotatedVisit that has the minimal set of fields required.
history::AnnotatedVisit CreateDefaultAnnotatedVisit(
    int visit_id,
    const GURL& url,
    base::Time visit_time = base::Time());

// Creates a ClusterVisit from |annotated_visit|. Will populate the returned
// ClusterVisit's normalized_url with |normalized_url| if present but otherwise
// will use the URL contained in the AnnotatedVisit.
history::ClusterVisit CreateClusterVisit(
    const history::AnnotatedVisit& annotated_visit,
    std::optional<GURL> normalized_url = std::nullopt,
    float score = 1.0,
    history::ClusterVisit::InteractionState interaction_state =
        history::ClusterVisit::InteractionState::kDefault);

history::DuplicateClusterVisit ClusterVisitToDuplicateClusterVisit(
    history::ClusterVisit cluster_visit);

// Creates a Cluster from |cluster_visits|.
history::Cluster CreateCluster(
    std::vector<history::ClusterVisit>& cluster_visits);

}  // namespace history_clusters::testing

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_CLUSTERING_TEST_UTILS_H_
