// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_POPULATIONS_POPULATION_H_
#define COMPONENTS_PRIVATE_INSIGHTS_POPULATIONS_POPULATION_H_

#include <cstddef>
#include <memory>
#include <string>

namespace private_insights {

class PendingEventsBatch;

// Prefix of every population's health histograms.
inline constexpr char kPopulationHistogramPrefix[] =
    "PrivateMetrics.PrivateInsights.";

// One Private Insights use case: one event proto type uploaded to its own
// Brella population. Owns its unsent events, configuration and histograms, so
// `PrivateInsightsService` can coordinate uploads without knowing the event
// types. Logged to and uploaded from the main sequence (see `EventQueue`).
//
// Concrete populations are `TypedPopulation<EventProto>` instances.
class Population {
 public:
  virtual ~Population() = default;

  // The Brella population to upload to. Read per upload.
  virtual std::string population_name() const = 0;

  // Number of events waiting to be uploaded.
  virtual size_t pending_event_count() const = 0;

  // Detaches all queued events; `PendingEventsBatch::Requeue()` returns them
  // here if the upload fails.
  virtual std::unique_ptr<PendingEventsBatch> TakePendingEvents() = 0;

  // Records the events that were never uploaded.
  virtual void OnShutdown() = 0;
};

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_POPULATIONS_POPULATION_H_
