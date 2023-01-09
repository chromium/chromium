// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_DELAYED_SAMPLER_H_
#define COMPONENTS_REPORTING_METRICS_DELAYED_SAMPLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

// A `Sampler` subclass that takes ownership of an existing `Sampler` and
// delays the collection of that sampler by `delay` amount
// of time.
class DelayedSampler : public Sampler {
 public:
  DelayedSampler(std::unique_ptr<Sampler> sampler, base::TimeDelta delay);

  DelayedSampler(const DelayedSampler&) = delete;
  DelayedSampler& operator=(const DelayedSampler&) = delete;

  ~DelayedSampler() override;

  // Waits until `delay_` amount of time has passed before collecting.
  void MaybeCollect(OptionalMetricCallback callback) override;

 private:
  // Used inside `MaybeCollect`. Collects data from `sampler_`
  void CollectInternal(OptionalMetricCallback callback);

  // The `Sampler` we actually collect data from
  std::unique_ptr<Sampler> sampler_;

  // Amount of time to delay collection by
  base::TimeDelta delay_;

  base::WeakPtrFactory<DelayedSampler> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_DELAYED_SAMPLER_H_
