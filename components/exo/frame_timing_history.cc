// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/frame_timing_history.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

namespace exo {
namespace {

constexpr size_t kRollingHistorySize = 60u;
constexpr double kFrameTransferDurationEstimationPercentile = 90.0;

// Reports metrics when the number of data points reaches this threshold.
constexpr int32_t kReportMetricsThreshold = 100;

}  // namespace

FrameTimingHistory::FrameTimingHistory()
    : frame_transfer_duration_history_(kRollingHistorySize) {}

FrameTimingHistory::~FrameTimingHistory() = default;

base::TimeDelta FrameTimingHistory::GetFrameTransferDurationEstimate() const {
  return frame_transfer_duration_history_.Percentile(
      kFrameTransferDurationEstimationPercentile);
}

void FrameTimingHistory::FrameSubmitted(uint32_t frame_token,
                                        base::TimeTicks submitted_time) {
  DCHECK(pending_submitted_time_.find(frame_token) ==
         pending_submitted_time_.end());

  pending_submitted_time_[frame_token] = submitted_time;

  RecordFrameResponseToRemote(/*did_not_produce=*/false);
  RecordFrameHandled(/*discarded=*/false);
}

void FrameTimingHistory::FrameDidNotProduce() {
  RecordFrameResponseToRemote(/*did_not_produce=*/true);
}

void FrameTimingHistory::FrameReceivedAtRemoteSide(
    uint32_t frame_token,
    base::TimeTicks received_time) {
  auto iter = pending_submitted_time_.find(frame_token);

  DCHECK(iter != pending_submitted_time_.end())
      << "Frame submitted time information is missing. Frame Token: "
      << frame_token;

  DCHECK_GE(received_time, iter->second);
  frame_transfer_duration_history_.InsertSample(received_time - iter->second);
  pending_submitted_time_.erase(iter);

  // FrameSubmitted() / FrameReceivedAtRemoteSide() are supposed to match, so
  // that the map won't grow indefinitely.
  DCHECK_LE(pending_submitted_time_.size(), 60u);
}

void FrameTimingHistory::FrameDiscarded() {
  RecordFrameHandled(/*discarded=*/true);
}

void FrameTimingHistory::MayRecordDidNotProduceToFrameArrvial(bool valid) {
  if (last_did_not_produce_time_.is_null()) {
    return;
  }

  base::TimeDelta duration =
      valid ? (base::TimeTicks::Now() - last_did_not_produce_time_)
            : base::TimeDelta();

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Graphics.Exo.Smoothness.DidNotProduceToFrameArrival", duration,
      base::Microseconds(1), base::Milliseconds(50), 50);

  last_did_not_produce_time_ = {};
}

void FrameTimingHistory::RecordFrameResponseToRemote(bool did_not_produce) {
  last_did_not_produce_time_ =
      did_not_produce ? base::TimeTicks::Now() : base::TimeTicks();

  frame_response_count_++;
  if (did_not_produce) {
    frame_response_did_not_produce_++;
  }

  if (frame_response_count_ >= kReportMetricsThreshold) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Exo.Smoothness.PercentDidNotProduceFrame",
        frame_response_did_not_produce_ * 100 / frame_response_count_);
    frame_response_count_ = 0;
    frame_response_did_not_produce_ = 0;
  }
}

void FrameTimingHistory::RecordFrameHandled(bool discarded) {
  frame_handling_count_++;
  if (discarded) {
    frame_handling_discarded_++;
  }

  if (frame_handling_count_ >= kReportMetricsThreshold) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Exo.Smoothness.PercentFrameDiscarded",
        frame_handling_discarded_ * 100 / frame_handling_count_);
    frame_handling_count_ = 0;
    frame_handling_discarded_ = 0;
  }
}

}  // namespace exo
