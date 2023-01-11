// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_legacymetrics/legacymetrics_client.h"

#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <zircon/errors.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/fuchsia_legacymetrics/legacymetrics_histogram_flattener.h"

namespace fuchsia_legacymetrics {

constexpr size_t LegacyMetricsClient::kMaxBatchSize;

constexpr base::TimeDelta LegacyMetricsClient::kInitialReconnectDelay;
constexpr base::TimeDelta LegacyMetricsClient::kMaxReconnectDelay;
constexpr size_t LegacyMetricsClient::kReconnectBackoffFactor;

LegacyMetricsClient::LegacyMetricsClient() = default;

LegacyMetricsClient::~LegacyMetricsClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LegacyMetricsClient::DisableAutoConnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(auto_connect_);
  DCHECK_EQ(report_interval_, base::TimeDelta())
      << "DisableAutoConnect() must be called before Start().";

  auto_connect_ = false;
}

void LegacyMetricsClient::SetMetricsRecorder(
    fidl::InterfaceHandle<fuchsia::legacymetrics::MetricsRecorder>
        metrics_recorder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!auto_connect_);

  auto weak_this = weak_factory_.GetWeakPtr();
  ResetMetricsRecorderState();

  // ResetMetricsRecorderState() may call |on_flush_complete_closures_|, which
  // may destroy LegacyMetricsClient.
  if (!weak_this)
    return;

  SetMetricsRecorderInternal(std::move(metrics_recorder));

  if (report_interval_.is_positive())
    ScheduleNextReport();
}

void LegacyMetricsClient::Start(base::TimeDelta report_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(report_interval, base::Seconds(0));

  // Start recording user events.
  user_events_recorder_ = std::make_unique<LegacyMetricsUserActionRecorder>();

  report_interval_ = report_interval;

  if (auto_connect_)
    ConnectFromComponentContext();

  if (metrics_recorder_)
    ScheduleNextReport();
}

void LegacyMetricsClient::SetReportAdditionalMetricsCallback(
    ReportAdditionalMetricsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!metrics_recorder_)
      << "SetReportAdditionalMetricsCallback() must be called before Start().";
  DCHECK(!report_additional_callback_);
  DCHECK(callback);

  report_additional_callback_ = std::move(callback);
}

void LegacyMetricsClient::SetNotifyFlushCallback(NotifyFlushCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  DCHECK(!metrics_recorder_)
      << "SetNotifyFlushCallback() must be called before Start().";

  notify_flush_callback_ = std::move(callback);
}

void LegacyMetricsClient::ConnectFromComponentContext() {
  DCHECK(!metrics_recorder_) << "Trying to connect when already connected.";
  DVLOG(1) << "Trying to connect to MetricsRecorder service.";
  DCHECK(auto_connect_);

  fidl::InterfaceHandle<fuchsia::legacymetrics::MetricsRecorder>
      metrics_recorder;
  base::ComponentContextForProcess()->svc()->Connect(
      metrics_recorder.NewRequest());
  SetMetricsRecorderInternal(std::move(metrics_recorder));

  ScheduleNextReport();
}

void LegacyMetricsClient::SetMetricsRecorderInternal(
    fidl::InterfaceHandle<fuchsia::legacymetrics::MetricsRecorder>
        metrics_recorder) {
  metrics_recorder_.Bind(std::move(metrics_recorder));
  metrics_recorder_.set_error_handler(fit::bind_member(
      this, &LegacyMetricsClient::OnMetricsRecorderDisconnected));
  metrics_recorder_.events().OnCloseSoon =
      fit::bind_member(this, &LegacyMetricsClient::OnCloseSoon);
}

void LegacyMetricsClient::ScheduleNextReport() {
  DCHECK(!is_flushing_);

  if (report_timer_.IsRunning())
    return;

  DVLOG(1) << "Scheduling next report in " << report_interval_.InSeconds()
           << "seconds.";
  report_timer_.Start(FROM_HERE, report_interval_, this,
                      &LegacyMetricsClient::StartReport);
}

void LegacyMetricsClient::StartReport() {
  if (!report_additional_callback_) {
    Report({});
    return;
  }
  report_additional_callback_.Run(
      base::BindOnce(&LegacyMetricsClient::Report, weak_factory_.GetWeakPtr()));
}

void LegacyMetricsClient::Report(
    std::vector<fuchsia::legacymetrics::Event> events) {
  DVLOG(1) << __func__ << " called.";

  // The connection might have dropped while additional metrics were being
  // collected. Continue recording events and cache them locally in memory until
  // connection is reestablished.
  if (!metrics_recorder_)
    return;

  // Include histograms.
  for (auto& histogram : GetLegacyMetricsDeltas()) {
    fuchsia::legacymetrics::Event histogram_event;
    histogram_event.set_histogram(std::move(histogram));
    events.push_back(std::move(histogram_event));
  }

  // Include user events.
  if (user_events_recorder_->HasEvents()) {
    for (auto& event : user_events_recorder_->TakeEvents()) {
      fuchsia::legacymetrics::Event user_event;
      user_event.set_user_action_event(std::move(event));
      events.push_back(std::move(user_event));
    }
  }

  std::move(events.begin(), events.end(), std::back_inserter(to_send_));

  DrainBuffer();
}

void LegacyMetricsClient::DrainBuffer() {
  DVLOG(1) << __func__ << " called.";

  if (record_ack_pending_) {
    // There is a Record() call already inflight. When it is acknowledged,
    // buffer draining will continue.
    return;
  }

  if (to_send_.empty()) {
    DVLOG(1) << "Buffer drained.";

    if (is_flushing_) {
      metrics_recorder_.Unbind();
      CompleteFlush();
      return;
    }

    ScheduleNextReport();
    return;
  }

  // Since ordering doesn't matter, we can efficiently drain |to_send_| by
  // repeatedly sending and truncating its tail.
  const size_t batch_size = std::min(to_send_.size(), kMaxBatchSize);
  const size_t batch_start_idx = to_send_.size() - batch_size;
  std::vector<fuchsia::legacymetrics::Event> batch;
  batch.resize(batch_size);
  std::move(to_send_.begin() + batch_start_idx, to_send_.end(), batch.begin());
  to_send_.resize(to_send_.size() - batch_size);

  record_ack_pending_ = true;
  metrics_recorder_->Record(std::move(batch), [this]() {
    record_ack_pending_ = false;

    // Reset the reconnect delay after a successful Record() call.
    reconnect_delay_ = kInitialReconnectDelay;

    DrainBuffer();
  });
}

void LegacyMetricsClient::OnMetricsRecorderDisconnected(zx_status_t status) {
  ZX_LOG(ERROR, status) << "MetricsRecorder connection lost.";

  if (auto_connect_ && status == ZX_ERR_PEER_CLOSED) {
    DVLOG(1) << "Scheduling reconnect after " << reconnect_delay_;

    // Try to reconnect with exponential backoff.
    reconnect_timer_.Start(FROM_HERE, reconnect_delay_, this,
                           &LegacyMetricsClient::ReconnectMetricsRecorder);

    // Increase delay exponentially. No random jittering since we don't expect
    // many clients overloading the service with simultaneous reconnections.
    reconnect_delay_ = std::min(reconnect_delay_ * kReconnectBackoffFactor,
                                kMaxReconnectDelay);
  }

  ResetMetricsRecorderState();
}

void LegacyMetricsClient::ReconnectMetricsRecorder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << __func__ << " called.";

  ConnectFromComponentContext();
  ScheduleNextReport();
}

void LegacyMetricsClient::FlushAndDisconnect(
    base::OnceClosure on_flush_complete) {
  DVLOG(1) << __func__ << " called.";

  if (on_flush_complete)
    on_flush_complete_closures_.push_back(std::move(on_flush_complete));

  if (is_flushing_)
    return;

  report_timer_.AbandonAndStop();

  is_flushing_ = true;
  if (notify_flush_callback_) {
    // Defer reporting until the flush operation has finished.
    std::move(notify_flush_callback_)
        .Run(base::BindOnce(&LegacyMetricsClient::StartReport,
                            weak_factory_.GetWeakPtr()));
  } else {
    StartReport();
  }
}

void LegacyMetricsClient::OnCloseSoon() {
  FlushAndDisconnect(base::OnceClosure());
}

void LegacyMetricsClient::CompleteFlush() {
  DCHECK(is_flushing_);

  is_flushing_ = false;

  // One of the callbacks may destroy |this|, so move them all to the stack
  // first.
  std::vector<base::OnceClosure> on_flush_complete_closures;
  on_flush_complete_closures.swap(on_flush_complete_closures_);
  for (auto& closure : on_flush_complete_closures) {
    std::move(closure).Run();
  }
}

void LegacyMetricsClient::ResetMetricsRecorderState() {
  // Stop reporting metric events.
  report_timer_.AbandonAndStop();

  record_ack_pending_ = false;

  if (is_flushing_)
    CompleteFlush();
}

}  // namespace fuchsia_legacymetrics
