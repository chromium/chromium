// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_FRAME_TIMING_HISTORY_H_
#define COMPONENTS_EXO_FRAME_TIMING_HISTORY_H_

#include <map>

#include "base/containers/small_map.h"
#include "base/time/time.h"
#include "cc/base/rolling_time_delta_history.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace exo {

// Records timing history of compositor frames and reports related metrics. Used
// by LayerTreeFrameSinkHolder to determine when to respond to BeginFrames.
class FrameTimingHistory {
 public:
  FrameTimingHistory();

  FrameTimingHistory(const FrameTimingHistory&) = delete;
  FrameTimingHistory& operator=(const FrameTimingHistory&) = delete;

  ~FrameTimingHistory();

  // Estimates the duration from LayerTreeFrameSinkHolder submitting a frame to
  // the remote side receiving it.
  base::TimeDelta GetFrameTransferDurationEstimate() const;

  // Notifies that a frame is submitted to the remote side.
  void FrameSubmitted(const viz::BeginFrameId& begin_frame_id,
                      uint32_t frame_token);
  // Notifies that a BeginFrame is responded with DidNotProduceFrame.
  void FrameDidNotProduce(const viz::BeginFrameId& begin_frame_id);
  // Notifies that a frame is received at the remote side.
  void FrameReceivedAtRemoteSide(uint32_t frame_token,
                                 base::TimeTicks received_time);

  // The number of DidNotProduceFrame responses since the last time when a frame
  // is submitted.
  int32_t consecutive_did_not_produce_count() const {
    return consecutive_did_not_produce_count_;
  }

 private:
  base::small_map<std::map<uint32_t, base::TimeTicks>> pending_submitted_time_;

  cc::RollingTimeDeltaHistory frame_transfer_duration_history_;

  int32_t consecutive_did_not_produce_count_ = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_FRAME_TIMING_HISTORY_H_
