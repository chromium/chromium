// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_REDIRECT_METRICS_RECORDER_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_REDIRECT_METRICS_RECORDER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/containers/lru_cache.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

namespace history {
class VisitRow;
}  // namespace history

namespace critical_actions {

// Records CriticalActions.EventLoggedInRedirect.{RedirectType}.{ActionSource}
// for critical actions whose visit was later redirected away to another visit.
//
// The owner only reports facts: every visit seen (`OnURLVisited()`) and every
// critical action resolved to a visit (`OnActionResolvedToVisit()`). Visits and
// actions may arrive in either order, so both are cached in capacity-limited
// LRU caches until they can be matched.
class CriticalActionRedirectMetricsRecorder {
 public:
  // `cache_capacity` is the maximum number of visits tracked by each cache.
  explicit CriticalActionRedirectMetricsRecorder(size_t cache_capacity);
  CriticalActionRedirectMetricsRecorder(
      const CriticalActionRedirectMetricsRecorder&) = delete;
  CriticalActionRedirectMetricsRecorder& operator=(
      const CriticalActionRedirectMetricsRecorder&) = delete;
  ~CriticalActionRedirectMetricsRecorder();

  // Called for every new visit. If it continues a redirect chain, marks its
  // referring visit as redirected and records the histogram for any actions
  // waiting on the referring visit.
  void OnURLVisited(const history::VisitRow& visit_row);

  // Called for every critical action resolved to `visit_id`. Records the
  // histogram if the visit is already known to be redirected, otherwise keeps
  // the action until a redirect away from the visit is seen.
  void OnActionResolvedToVisit(int64_t visit_id,
                               ActionSource source,
                               ActionType action_type);

  // Drops all cached visits and actions without recording.
  void Clear();

 private:
  // Type of redirect that took a visit to another visit.
  //
  // New values (e.g. "internal" redirects) must also be handled in the
  // classification and histogram code linked below.
  // LINT.IfChange(RedirectType)
  enum class RedirectType {
    kServer,
    kClient,
  };
  // LINT.ThenChange(
  //     //components/critical_actions/core/browser/critical_action_redirect_metrics_recorder.cc:RedirectTypeClassification,
  //     //tools/metrics/histograms/metadata/critical_actions/histograms.xml:CriticalActionRedirectType
  // )

  // A critical action logged on a visit, cached in `visits_to_actions_cache_`.
  struct VisitAction {
    ActionSource action_source;
    ActionType action_type;
  };

  // Records CriticalActions.EventLoggedInRedirect.{RedirectType}.{ActionSource}
  // for a critical action of `action_type` from `source` whose visit was
  // redirected away by `redirect_type`.
  static void RecordEventLoggedInRedirectHistogram(RedirectType redirect_type,
                                                   ActionSource source,
                                                   ActionType action_type);

  // Capacity-limited LRU cache of visits that have critical actions but are
  // not yet known to have been redirected. Maps visit_id to the actions
  // logged on it whose recording is deferred.
  base::LRUCache<int64_t, std::vector<VisitAction>> visits_to_actions_cache_;

  // Capacity-limited LRU cache of visits known to have been redirected to
  // another visit, regardless of whether they have critical actions yet. Maps
  // visit_id to the redirect type (server or client).
  base::LRUCache<int64_t, RedirectType> visit_redirect_types_;
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_REDIRECT_METRICS_RECORDER_H_
