// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_FRAME_TIMING_HISTORY_H_
#define COMPONENTS_EXO_FRAME_TIMING_HISTORY_H_

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "cc/base/rolling_time_delta_history.h"

namespace exo {

// Records timing history of compositor frames and reports related metrics. Used
// by LayerTreeFrameSinkHolder to determine when to respond to BeginFrame
// requests.
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
  void FrameSubmitted(uint32_t frame_token, base::TimeTicks submitted_time);
  // Notifies that a frame request is responded with DidNotProduceFrame.
  void FrameDidNotProduce();
  // Notifies that a frame is received at the remote side.
  void FrameReceivedAtRemoteSide(uint32_t frame_token,
                                 base::TimeTicks received_time);
  // Notifies that a frame is discarded locally.
  void FrameDiscarded();

  // Records a value for DidNotProduceToFrameArrival histogram if applicable.
  // For each DidNotProduceFrame response, the first call to this method
  // reports one value; the subsequent calls (if any) are ignored.
  //   - `valid` set to false indicates that either (1) DidNotProduceFrame is
  //     issued when there are already queued BeginFrame requests; or (2) a new
  //     BeginFrame request arrives before the next frame. In this case the
  //     value reported is 0.
  //   - `valid` set to true: called at the next frame arrival time, reporting
  //     the duration between sending the DidNotProduceFrame response and the
  //     next frame arrival.
  void MayRecordDidNotProduceToFrameArrvial(bool valid);

 private:
  void RecordFrameResponseToRemote(bool did_not_produce);
  void RecordFrameHandled(bool discarded);

  base::flat_map<uint32_t, base::TimeTicks> pending_submitted_time_;

  cc::RollingTimeDeltaHistory frame_transfer_duration_history_;

  // Records the time of sending the last DidNotProduceFrame response. It is
  // used to report DidNotProduceToFrameArrival metric and then reset.
  base::TimeTicks last_did_not_produce_time_;

  // Counters used to report metrics.
  int32_t frame_response_count_ = 0;
  int32_t frame_response_did_not_produce_ = 0;
  int32_t frame_handling_count_ = 0;
  int32_t frame_handling_discarded_ = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_FRAME_TIMING_HISTORY_H_
