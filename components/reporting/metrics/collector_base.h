// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_COLLECTOR_BASE_H_
#define COMPONENTS_REPORTING_METRICS_COLLECTOR_BASE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

class Sampler;

// A base class for metric data collection.
class CollectorBase {
 public:
  explicit CollectorBase(Sampler* sampler);

  CollectorBase(const CollectorBase& other) = delete;
  CollectorBase& operator=(const CollectorBase& other) = delete;

  virtual ~CollectorBase();

  // Collect metric data provided by `sampler_` asynchronously.
  virtual void Collect(bool is_event_driven);

 protected:
  // Callback executed when metric data is collected.
  virtual void OnMetricDataCollected(bool is_event_driven,
                                     std::optional<MetricData> metric_data) = 0;

  void CheckOnSequence() const;

  virtual bool CanCollect() const = 0;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<Sampler> sampler_;

  base::WeakPtrFactory<CollectorBase> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_COLLECTOR_BASE_H_
