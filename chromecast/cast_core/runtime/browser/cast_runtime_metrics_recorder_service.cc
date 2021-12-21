// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder_service.h"

#include "base/logging.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_action_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_histogram_flattener.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder.h"
#include "chromecast/cast_core/runtime/browser/metrics_recorder_grpc.h"
#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.pb.h"

namespace chromecast {
namespace {

constexpr size_t kMaxBatchSize = 50;

}  // namespace

CastRuntimeMetricsRecorderService::CastRuntimeMetricsRecorderService(
    CastRuntimeMetricsRecorder* metrics_recorder,
    CastRuntimeActionRecorder* action_recorder,
    MetricsRecorderGrpc* metrics_recorder_grpc,
    base::TimeDelta report_interval)
    : metrics_recorder_(metrics_recorder),
      action_recorder_(action_recorder),
      metrics_recorder_grpc_(metrics_recorder_grpc) {
  report_timer_.Start(
      FROM_HERE, report_interval,
      base::BindRepeating(&CastRuntimeMetricsRecorderService::Report,
                          base::Unretained(this)));
  metrics_recorder_grpc_->SetClient(this);
}

CastRuntimeMetricsRecorderService::~CastRuntimeMetricsRecorderService() {
  metrics_recorder_grpc_->SetClient(nullptr);
}

void CastRuntimeMetricsRecorderService::OnRecordComplete() {
  DCHECK(ack_pending_);
  ack_pending_ = false;
  DrainBuffer();
}

void CastRuntimeMetricsRecorderService::OnCloseSoon(
    base::OnceClosure complete_callback) {
  if (flush_complete_callback_) {
    return;
  }
  report_timer_.AbandonAndStop();
  flush_complete_callback_ = std::move(complete_callback);
  Report();
}

void CastRuntimeMetricsRecorderService::Report() {
  std::vector<cast::metrics::Event> events = metrics_recorder_->TakeEvents();
  for (auto& histogram : GetHistogramDeltas()) {
    cast::metrics::Event event;
    *event.mutable_histogram() = std::move(histogram);
    events.push_back(std::move(event));
  }

  for (auto& action : action_recorder_->TakeEvents()) {
    cast::metrics::Event event;
    *event.mutable_user_action_event() = std::move(action);
    events.push_back(std::move(event));
  }

  std::move(events.begin(), events.end(), std::back_inserter(send_buffer_));

  DrainBuffer();
}

void CastRuntimeMetricsRecorderService::DrainBuffer() {
  if (ack_pending_) {
    return;
  }
  if (send_buffer_.empty()) {
    if (flush_complete_callback_) {
      std::move(flush_complete_callback_).Run();
    }
    return;
  }
  // NOTE: Ordering doesn't matter, so we can more efficiently tear off the end
  // of the vector each time.
  size_t start_index =
      send_buffer_.size() - std::min(send_buffer_.size(), kMaxBatchSize);
  cast::metrics::RecordRequest request;
  *request.mutable_event() = {send_buffer_.begin() + start_index,
                              send_buffer_.end()};
  send_buffer_.resize(start_index);
  DVLOG(2) << "Sending metrics";
  ack_pending_ = true;
  metrics_recorder_grpc_->Record(request);
}

}  // namespace chromecast
