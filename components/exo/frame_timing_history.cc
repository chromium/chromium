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

}  // namespace

FrameTimingHistory::FrameTimingHistory()
    : frame_transfer_duration_history_(kRollingHistorySize) {}

FrameTimingHistory::~FrameTimingHistory() = default;

base::TimeDelta FrameTimingHistory::GetFrameTransferDurationEstimate() const {
  return frame_transfer_duration_history_.Percentile(
      kFrameTransferDurationEstimationPercentile);
}

void FrameTimingHistory::FrameSubmitted(const viz::BeginFrameId& begin_frame_id,
                                        uint32_t frame_token) {
  DCHECK(pending_submitted_time_.find(frame_token) ==
         pending_submitted_time_.end());

  base::TimeTicks submitted_time = base::TimeTicks::Now();
  pending_submitted_time_[frame_token] = submitted_time;

  consecutive_did_not_produce_count_ = 0;
}

void FrameTimingHistory::FrameDidNotProduce(const viz::BeginFrameId& id) {
  consecutive_did_not_produce_count_++;
}

void FrameTimingHistory::FrameReceivedAtRemoteSide(
    uint32_t frame_token,
    base::TimeTicks received_time) {
  auto iter = pending_submitted_time_.find(frame_token);

  CHECK(iter != pending_submitted_time_.end())
      << "Frame submitted time information is missing. Frame Token: "
      << frame_token;

  DCHECK_GE(received_time, iter->second);
  frame_transfer_duration_history_.InsertSample(received_time - iter->second);
  pending_submitted_time_.erase(iter);

  // FrameSubmitted() / FrameReceivedAtRemoteSide() are supposed to match, so
  // that the map won't grow indefinitely.
  DCHECK_LE(pending_submitted_time_.size(), 60u);
}

}  // namespace exo
