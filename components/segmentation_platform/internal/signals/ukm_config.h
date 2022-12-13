// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_UKM_CONFIG_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_UKM_CONFIG_H_

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"

namespace segmentation_platform {

// Config for observation and storage of UKM.
class UkmConfig {
 public:
  using EventsToMetricsMap =
      base::flat_map<UkmEventHash, base::flat_set<UkmMetricHash>>;

  UkmConfig();
  ~UkmConfig();

  UkmConfig(const UkmConfig&) = delete;
  UkmConfig& operator=(const UkmConfig&) = delete;

  // Merge all the events from the given |config|. Returns whether new UKM
  // events were added to the current config as a result of merging.
  enum MergeResult {
    // A new event hash that was not being observed, was added to the config as
    // a result of the merge.
    NEW_EVENT_ADDED,
    // No new event hash was added. NO_NEW_EVENT is returned even if new metrics
    // were added to existing event hashes.
    NO_NEW_EVENT
  };
  MergeResult Merge(const UkmConfig& config);

  // Returns a list of observed event hashes, for sending it to UkmRecorderImpl.
  base::flat_set<uint64_t> GetRawObservedEvents() const;

  // Returns a list of metrics observed for the given |event|. Returns nullptr
  // if the event is not observed.
  const base::flat_set<UkmMetricHash>* GetObservedMetrics(UkmEventHash event);

  // Add the given event and metrics for observation.
  void AddEvent(UkmEventHash event_hash,
                const base::flat_set<UkmMetricHash>& metrics);

  const EventsToMetricsMap& metrics_for_event_for_testing() const {
    return metrics_for_event_;
  }

  bool operator==(const UkmConfig& other) const;

 private:
  EventsToMetricsMap metrics_for_event_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_UKM_CONFIG_H_
