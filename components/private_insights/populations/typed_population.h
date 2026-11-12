// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_POPULATIONS_TYPED_POPULATION_H_
#define COMPONENTS_PRIVATE_INSIGHTS_POPULATIONS_TYPED_POPULATION_H_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/private_insights/event_queue.h"
#include "components/private_insights/populations/pending_events_batch.h"
#include "components/private_insights/populations/population.h"

namespace private_insights {

// `Population` for one event proto. Owns the proto's `EventQueue`; the
// population name and queue capacity are Finch params, re-read on every use.
//
// Health histograms are `kPopulationHistogramPrefix` + `histogram_name` +
// the `EventQueue` suffixes, plus `.Logging.OnShutdownCount` (events still
// queued at shutdown).
template <typename EventProto>
class TypedPopulation final : public Population {
 public:
  using Queue = EventQueue<EventProto>;

  // The params must outlive the population.
  TypedPopulation(std::string_view histogram_name,
                  const base::FeatureParam<std::string>& population_name_param,
                  const base::FeatureParam<int>& max_events_param)
      : histogram_prefix_(
            base::StrCat({kPopulationHistogramPrefix, histogram_name})),
        queue_(histogram_prefix_),
        population_name_param_(population_name_param),
        max_events_param_(max_events_param) {}

  TypedPopulation(const TypedPopulation&) = delete;
  TypedPopulation& operator=(const TypedPopulation&) = delete;

  ~TypedPopulation() override = default;

  // Queues `event` for the next upload.
  void LogEvent(EventProto event) {
    queue_.Push(std::move(event), max_events_param_->Get());
  }

  // Population:
  std::string population_name() const override {
    return population_name_param_->Get();
  }

  size_t pending_event_count() const override { return queue_.size(); }

  std::unique_ptr<PendingEventsBatch> TakePendingEvents() override {
    // Snapshot the cap so a requeue after a failed upload uses the limit in
    // effect when the batch was taken. The lambda checks the WeakPtr itself:
    // `BindOnce` only auto-cancels for member function targets.
    return std::make_unique<TypedPendingEventsBatch<EventProto>>(
        queue_.TakeAll(), base::BindOnce(
                              [](base::WeakPtr<Queue> queue, int max_events,
                                 typename Queue::Entries events) {
                                if (queue) {
                                  queue->Requeue(std::move(events), max_events);
                                }
                              },
                              queue_.GetWeakPtr(), max_events_param_->Get()));
  }

  void OnShutdown() override {
    base::UmaHistogramCounts100(
        base::StrCat({histogram_prefix_, ".Logging.OnShutdownCount"}),
        static_cast<int>(queue_.size()));
  }

  const Queue& queue_for_testing() const { return queue_; }

 private:
  const std::string histogram_prefix_;
  Queue queue_;
  const raw_ref<const base::FeatureParam<std::string>> population_name_param_;
  const raw_ref<const base::FeatureParam<int>> max_events_param_;
};

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_POPULATIONS_TYPED_POPULATION_H_
