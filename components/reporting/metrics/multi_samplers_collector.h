// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_MULTI_SAMPLERS_COLLECTOR_H_
#define COMPONENTS_REPORTING_METRICS_MULTI_SAMPLERS_COLLECTOR_H_

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

class ConfiguredSampler;

// Class to collect data from multiple samplers async. Response callback is
// invoked on destruction. Each sampling callback passed to a sampler will hold
// reference to the class instance, so the final class instance response will
// not be invoked until the class created instance reference is released and
// each callback is done (either by being run or dropped).
class MultiSamplersCollector
    : public base::RefCountedDeleteOnSequence<MultiSamplersCollector> {
 public:
  static void CollectAll(
      const std::vector<ConfiguredSampler*>& configured_samplers,
      OptionalMetricCallback metric_callback);

  explicit MultiSamplersCollector(OptionalMetricCallback metric_callback);

  MultiSamplersCollector(const MultiSamplersCollector& other) = delete;
  MultiSamplersCollector& operator=(const MultiSamplersCollector& other) =
      delete;

  void Collect(Sampler* sampler);

 private:
  friend class base::RefCountedDeleteOnSequence<MultiSamplersCollector>;
  friend class base::DeleteHelper<MultiSamplersCollector>;

  void MergeMetricData(absl::optional<MetricData> new_metric_data);

  ~MultiSamplersCollector();

  SEQUENCE_CHECKER(sequence_checker_);

  absl::optional<MetricData> metric_data_ GUARDED_BY_CONTEXT(sequence_checker_);

  OptionalMetricCallback metric_callback_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_METRICS_MULTI_SAMPLERS_COLLECTOR_H_
