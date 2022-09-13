// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/reporting/metrics/configured_sampler.h"
#include "components/reporting/metrics/multi_samplers_collector.h"
#include "components/reporting/metrics/sampler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

// static
void MultiSamplersCollector::CollectAll(
    const std::vector<ConfiguredSampler*>& configured_samplers,
    OptionalMetricCallback metric_callback) {
  auto multi_collector =
      base::MakeRefCounted<MultiSamplersCollector>(std::move(metric_callback));
  for (auto* sampler : configured_samplers) {
    if (sampler->IsReportingEnabled()) {
      multi_collector->Collect(sampler->GetSampler());
    }
  }
}

MultiSamplersCollector::MultiSamplersCollector(
    OptionalMetricCallback metric_callback)
    : base::RefCountedDeleteOnSequence<MultiSamplersCollector>(
          base::SequencedTaskRunnerHandle::Get()),
      metric_callback_(std::move(metric_callback)) {}

void MultiSamplersCollector::Collect(Sampler* sampler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto on_collected_cb =
      base::BindOnce(&MultiSamplersCollector::MergeMetricData, this);
  sampler->MaybeCollect(base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(on_collected_cb)));
}

void MultiSamplersCollector::MergeMetricData(
    absl::optional<MetricData> new_metric_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!new_metric_data.has_value()) {
    return;
  }
  if (!metric_data_.has_value()) {
    metric_data_ = std::move(new_metric_data);
    return;
  }

  metric_data_->CheckTypeAndMergeFrom(new_metric_data.value());
}

MultiSamplersCollector::~MultiSamplersCollector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(metric_callback_).Run(std::move(metric_data_));
}

}  // namespace reporting
