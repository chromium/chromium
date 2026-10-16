// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "url/gurl.h"

namespace optimization_guide {

// The statuses for racing a hints fetch with the current navigation based
// on the availability of hints for both the current host and URL.
//
// Keep in sync with OptimizationGuideRaceNavigationFetchAttemptStatus in
// enums.xml.
enum class RaceNavigationFetchAttemptStatus {
  kUnknown,
  // The race was not attempted because hint information for the host and URL
  // of the current navigation was already available.
  kRaceNavigationFetchNotAttempted,
  // The race was attempted for the host of the current navigation but not the
  // URL.
  kRaceNavigationFetchHost,
  // The race was attempted for the URL of the current navigation but not the
  // host.
  kRaceNavigationFetchURL,
  // The race was attempted for the host and URL of the current navigation.
  kRaceNavigationFetchHostAndURL,
  // A race for the current navigation's URL is already in progress.
  kRaceNavigationFetchAlreadyInProgress,
  // DEPRECATED: A race for the current navigation's URL was not attempted
  // because there were too many concurrent page navigation fetches in flight.
  kDeprecatedRaceNavigationFetchNotAttemptedTooManyConcurrentFetches,

  // Add new values above this line.
  kMaxValue =
      kDeprecatedRaceNavigationFetchNotAttemptedTooManyConcurrentFetches,
};

}  // namespace optimization_guide

// A representation of optimization guide information related to a navigation.
// Metrics will be recorded upon this object's destruction.
class OptimizationGuideNavigationData {
 public:
  OptimizationGuideNavigationData(int64_t navigation_id,
                                  base::TimeTicks navigation_start);
  ~OptimizationGuideNavigationData();

  OptimizationGuideNavigationData(
      const OptimizationGuideNavigationData& other) = delete;
  OptimizationGuideNavigationData& operator=(
      const OptimizationGuideNavigationData&) = delete;

  base::WeakPtr<OptimizationGuideNavigationData> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // The navigation ID of the navigation handle that this data is
  // associated with.
  int64_t navigation_id() const { return navigation_id_; }

  // The time of navigation start.
  base::TimeTicks navigation_start() const { return navigation_start_; }

  // The navigation URL of the navigation handle that this data is
  // associated with.
  GURL navigation_url() const { return navigation_url_; }
  void set_navigation_url(const GURL& navigation_url) {
    navigation_url_ = navigation_url;
  }

  // The optimization types that were registered at the start of the navigation.
  base::flat_set<optimization_guide::proto::OptimizationType>
  registered_optimization_types() const {
    return registered_optimization_types_;
  }
  void set_registered_optimization_types(
      base::flat_set<optimization_guide::proto::OptimizationType>
          registered_optimization_types) {
    registered_optimization_types_ = registered_optimization_types;
  }

  // The duration between the fetch for a hint for the navigation going out to
  // when it was received by the client if a fetch was initiated for the
  // navigation.
  std::optional<base::TimeDelta> hints_fetch_latency() const;
  void set_hints_fetch_start(base::TimeTicks hints_fetch_start) {
    hints_fetch_start_ = hints_fetch_start;
  }
  void set_hints_fetch_end(base::TimeTicks hints_fetch_end) {
    hints_fetch_end_ = hints_fetch_end;
  }

  // The status for whether a hint for the page load was attempted to be fetched
  // from the remote Optimization Guide Service.
  std::optional<optimization_guide::RaceNavigationFetchAttemptStatus>
  hints_fetch_attempt_status() const {
    return hints_fetch_attempt_status_;
  }
  void set_hints_fetch_attempt_status(
      optimization_guide::RaceNavigationFetchAttemptStatus
          hints_fetch_attempt_status) {
    hints_fetch_attempt_status_ = hints_fetch_attempt_status;
  }

 private:
  // Records metrics based on data currently held in |this|.
  void RecordMetrics() const;

  // Records the OptimizationGuide UKM event based on data currently held in
  // |this|.
  void RecordOptimizationGuideUKM() const;

  // The navigation ID of the navigation handle that this data is associated
  // with.
  const int64_t navigation_id_;

  // The time of navigation start.
  const base::TimeTicks navigation_start_;

  // The navigation URL of the navigation handle this data is associated with.
  // Updated on navigation start and navigation redirects.
  GURL navigation_url_;

  // The optimization types that were registered at the start of the navigation.
  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types_;

  // The time that the hints fetch for this navigation started. Is only present
  // if a fetch was initiated for this navigation.
  std::optional<base::TimeTicks> hints_fetch_start_;

  // The time that the hints fetch for the navigation ended. Is only present if
  // a fetch was initiated and successfully completed for this navigation.
  std::optional<base::TimeTicks> hints_fetch_end_;

  // The status for whether a hint for the page load was attempted to be fetched
  // from the remote Optimization Guide Service.
  std::optional<optimization_guide::RaceNavigationFetchAttemptStatus>
      hints_fetch_attempt_status_;

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<OptimizationGuideNavigationData> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_OPTIMIZATION_GUIDE_NAVIGATION_DATA_H_
