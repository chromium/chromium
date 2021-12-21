// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_RECORDER_SERVICE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_RECORDER_SERVICE_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/cast_core/runtime/browser/metrics_recorder_grpc.h"

namespace chromecast {

class CastRuntimeActionRecorder;
class CastRuntimeMetricsRecorder;

// This class uses a timer to periodically send all collected metrics to Cast
// Core via MetricsRecorderGrpc.  It begins running the timer task on
// construction.  After OnCloseSoon(), it only attempts one more round of
// metrics and then stops.
class CastRuntimeMetricsRecorderService final
    : public MetricsRecorderGrpc::Client {
 public:
  // All these pointers must outlive |this|.
  CastRuntimeMetricsRecorderService(
      CastRuntimeMetricsRecorder* metrics_recorder,
      CastRuntimeActionRecorder* action_recorder,
      MetricsRecorderGrpc* metrics_recorder_grpc,
      base::TimeDelta report_interval);
  ~CastRuntimeMetricsRecorderService() override;

  void OnRecordComplete() override;
  void OnCloseSoon(base::OnceClosure complete_callback) override;

 private:
  void Report();
  void DrainBuffer();

  CastRuntimeMetricsRecorder* const metrics_recorder_;
  CastRuntimeActionRecorder* const action_recorder_;
  MetricsRecorderGrpc* const metrics_recorder_grpc_;
  base::RepeatingTimer report_timer_;

  bool ack_pending_{false};
  base::OnceClosure flush_complete_callback_;
  std::vector<cast::metrics::Event> send_buffer_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_RECORDER_SERVICE_H_
