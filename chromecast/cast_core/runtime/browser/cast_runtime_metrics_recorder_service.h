// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_RECORDER_SERVICE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_RECORDER_SERVICE_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.pb.h"

namespace chromecast {

class CastRuntimeActionRecorder;
class CastRuntimeMetricsRecorder;

// This class uses a timer to periodically send all collected metrics to Cast
// Core via MetricsRecorderGrpc.  It begins running the timer task on
// construction.  After OnCloseSoon(), it only attempts one more round of
// metrics and then stops.
class CastRuntimeMetricsRecorderService {
 public:
  using RecordCompleteCallback = base::OnceClosure;
  using RecordMetricsCallback =
      base::RepeatingCallback<void(cast::metrics::RecordRequest,
                                   RecordCompleteCallback)>;

  // All these pointers must outlive |this|.
  CastRuntimeMetricsRecorderService(
      CastRuntimeMetricsRecorder* metrics_recorder,
      CastRuntimeActionRecorder* action_recorder,
      RecordMetricsCallback record_metrics_callback,
      base::TimeDelta report_interval);
  ~CastRuntimeMetricsRecorderService();

  void OnCloseSoon(base::OnceClosure complete_callback);

 private:
  void Report();
  void DrainBuffer();
  void OnMetricsRecorded();

  CastRuntimeMetricsRecorder* const metrics_recorder_;
  CastRuntimeActionRecorder* const action_recorder_;
  RecordMetricsCallback record_metrics_callback_;
  base::RepeatingTimer report_timer_;

  bool ack_pending_{false};
  base::OnceClosure flush_complete_callback_;
  std::vector<cast::metrics::Event> send_buffer_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastRuntimeMetricsRecorderService> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_RECORDER_SERVICE_H_
