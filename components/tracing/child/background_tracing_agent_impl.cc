// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/child/background_tracing_agent_impl.h"

#include <memory>

#include "base/metrics/statistics_recorder.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace tracing {
namespace {

constexpr base::TimeDelta kMinTimeBetweenHistogramChanges =
    base::TimeDelta::FromSeconds(10);

}  // namespace

BackgroundTracingAgentImpl::BackgroundTracingAgentImpl(
    mojo::PendingRemote<mojom::BackgroundTracingAgentClient> client)
    : client_(std::move(client)) {
  client_->OnInitialized();
}

BackgroundTracingAgentImpl::~BackgroundTracingAgentImpl() = default;

void BackgroundTracingAgentImpl::SetUMACallback(
    const std::string& histogram_name,
    int32_t histogram_lower_value,
    int32_t histogram_upper_value,
    bool repeat) {
  histogram_last_changed_ = base::Time();

  base::WeakPtr<BackgroundTracingAgentImpl> weak_self =
      weak_factory_.GetWeakPtr();

  // This callback will run on a random thread, so we need to proxy back to the
  // current sequence before touching |this|.
  base::StatisticsRecorder::SetCallback(
      histogram_name,
      base::BindRepeating(&BackgroundTracingAgentImpl::OnHistogramChanged,
                          weak_self, base::SequencedTaskRunnerHandle::Get(),
                          histogram_name, histogram_lower_value,
                          histogram_upper_value, repeat));

  base::HistogramBase* existing_histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);
  if (!existing_histogram)
    return;

  std::unique_ptr<base::HistogramSamples> samples =
      existing_histogram->SnapshotSamples();
  if (!samples)
    return;

  std::unique_ptr<base::SampleCountIterator> sample_iterator =
      samples->Iterator();
  if (!sample_iterator)
    return;

  while (!sample_iterator->Done()) {
    base::HistogramBase::Sample min;
    int64_t max;
    base::HistogramBase::Count count;
    sample_iterator->Get(&min, &max, &count);

    if (min >= histogram_lower_value && max <= histogram_upper_value) {
      SendTriggerMessage(histogram_name);
      break;
    }
    if (!repeat) {
      SendAbortBackgroundTracingMessage();
      break;
    }

    sample_iterator->Next();
  }
}

void BackgroundTracingAgentImpl::ClearUMACallback(
    const std::string& histogram_name) {
  histogram_last_changed_ = base::Time();
  base::StatisticsRecorder::ClearCallback(histogram_name);
}

// static
void BackgroundTracingAgentImpl::OnHistogramChanged(
    base::WeakPtr<BackgroundTracingAgentImpl> weak_self,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::string& histogram_name,
    base::Histogram::Sample histogram_lower_value,
    base::Histogram::Sample histogram_upper_value,
    bool repeat,
    base::Histogram::Sample actual_value) {
  // NOTE: This method is called from an arbitrary sequence.

  if (actual_value < histogram_lower_value ||
      actual_value > histogram_upper_value) {
    if (!repeat) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BackgroundTracingAgentImpl::SendAbortBackgroundTracingMessage,
              weak_self));
    }
    return;
  }

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundTracingAgentImpl::SendTriggerMessage,
                                weak_self, histogram_name));
}

void BackgroundTracingAgentImpl::SendTriggerMessage(
    const std::string& histogram_name) {
  base::Time now = TRACE_TIME_NOW();

  if (!histogram_last_changed_.is_null()) {
    base::Time computed_next_allowed_time =
        histogram_last_changed_ + kMinTimeBetweenHistogramChanges;
    if (computed_next_allowed_time > now)
      return;
  }
  histogram_last_changed_ = now;

  client_->OnTriggerBackgroundTrace(histogram_name);
}

void BackgroundTracingAgentImpl::SendAbortBackgroundTracingMessage() {
  client_->OnAbortBackgroundTrace();
}

}  // namespace tracing
