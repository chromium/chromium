// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_TYPES_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_TYPES_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/history/core/browser/history_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace history_clusters {

struct QueryClustersContinuationParams {
  QueryClustersContinuationParams() = default;
  QueryClustersContinuationParams(base::Time continuation_time,
                                  bool is_continuation,
                                  bool is_partial_day,
                                  bool exhausted_unclustered_visits,
                                  bool exhausted_all_visits)
      : continuation_time(continuation_time),
        is_continuation(is_continuation),
        is_partial_day(is_partial_day),
        exhausted_unclustered_visits(exhausted_unclustered_visits),
        exhausted_all_visits(exhausted_all_visits) {}

  // Returns a `QueryClustersContinuationParams` representing the done state.
  // Most of the values don't matter, but `exhausted_unclustered_visits` and
  // `exhausted_all_visits` should be true.
  static const QueryClustersContinuationParams DoneParams() {
    static QueryClustersContinuationParams kDoneParams = {base::Time(), true,
                                                          false, true, true};
    return kDoneParams;
  }

  // The time already fetched visits up to and where the next request will
  // continue.
  base::Time continuation_time = base::Time();
  // False for the first request, true otherwise.
  bool is_continuation = false;
  // True if left off midday; i.e. not a day boundary. This occurs when the max
  // visit threshold was reached.
  bool is_partial_day = false;
  // True if unclustered visits were exhausted. If we're searching oldest to
  // newest, this is true iff `exhausted_all_visits` is true. Otherwise, this
  // may be true before `exhausted_all_visits` is true but not the reverse.
  bool exhausted_unclustered_visits = false;
  // True if both unclustered and clustered were exhausted.
  bool exhausted_all_visits = false;
};

using QueryClustersCallback =
    base::OnceCallback<void(std::vector<history::Cluster>,
                            QueryClustersContinuationParams)>;

// Tracks which fields have been or are pending recording. This helps 1) avoid
// re-recording fields and 2) determine whether a visit is complete (i.e. has
// all expected fields recorded).
struct RecordingStatus {
  // Whether `url_row` and `visit_row` have been set.
  bool history_rows = false;
  // Whether a navigation has ended; i.e. another navigation has begun in the
  // same tab or the navigation's tab has been closed.
  bool navigation_ended = false;
  // Whether the `context_annotations` associated with navigation end have been
  // set. Should only be true if both `history_rows` and `navigation_ended` are
  // true.
  bool navigation_end_signals = false;
  // Whether the UKM `page_end_reason` `context_annotations` is expected to be
  // set.
  bool expect_ukm_page_end_signals = false;
  // Whether the UKM `page_end_reason` `context_annotations` has been set.
  // Should only be true if `expect_ukm_page_end_signals` is true.
  bool ukm_page_end_signals = false;
};

// A partially built VisitContextAnnotations with its state of completeness and
// associated `URLRow` and `VisitRow` which are necessary to build it.
struct IncompleteVisitContextAnnotations {
  IncompleteVisitContextAnnotations();
  IncompleteVisitContextAnnotations(const IncompleteVisitContextAnnotations&);
  ~IncompleteVisitContextAnnotations();

  RecordingStatus status;
  history::URLRow url_row;
  history::VisitRow visit_row;
  history::VisitContextAnnotations context_annotations;
};

// Used to track incomplete, unpersisted visits.
using IncompleteVisitMap = std::map<int64_t, IncompleteVisitContextAnnotations>;

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_TYPES_H_
