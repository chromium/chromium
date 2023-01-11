// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_LEGACYMETRICS_LEGACYMETRICS_CLIENT_H_
#define COMPONENTS_FUCHSIA_LEGACYMETRICS_LEGACYMETRICS_CLIENT_H_

#include <fuchsia/legacymetrics/cpp/fidl.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/fuchsia_legacymetrics/legacymetrics_user_event_recorder.h"

namespace fuchsia_legacymetrics {

// Used to report events & histogram data to the
// fuchsia.legacymetrics.MetricsRecorder service.
// LegacyMetricsClient must be Start()ed on an IO-capable sequence.
// Cannot be used in conjunction with other metrics reporting services.
// Must be constructed, used, and destroyed on the same sequence.
class LegacyMetricsClient {
 public:
  // Maximum number of Events to send to Record() at a time, so as to not exceed
  // the 64KB FIDL maximum message size.
  static constexpr size_t kMaxBatchSize = 50;

  // Constants for FIDL reconnection with exponential backoff.
  static constexpr base::TimeDelta kInitialReconnectDelay = base::Seconds(1);
  static constexpr base::TimeDelta kMaxReconnectDelay = base::Hours(1);
  static constexpr size_t kReconnectBackoffFactor = 2;

  using ReportAdditionalMetricsCallback = base::RepeatingCallback<void(
      base::OnceCallback<void(std::vector<fuchsia::legacymetrics::Event>)>)>;
  using NotifyFlushCallback =
      base::OnceCallback<void(base::OnceClosure completion_cb)>;

  LegacyMetricsClient();
  ~LegacyMetricsClient();

  explicit LegacyMetricsClient(const LegacyMetricsClient&) = delete;
  LegacyMetricsClient& operator=(const LegacyMetricsClient&) = delete;

  // Disables automatic MetricsRecorder connection. Caller will have to supply
  // MetricsRecorder by calling SetMetricsRecorder(). Must be called before
  // Start().
  void DisableAutoConnect();

  // Sets |metrics_recorder| to use. Should be called only after
  // DisableAutoConnect().
  void SetMetricsRecorder(
      fidl::InterfaceHandle<fuchsia::legacymetrics::MetricsRecorder>
          metrics_recorder);

  // Starts buffering data and schedules metric reporting after every
  // |report_interval|.
  void Start(base::TimeDelta report_interval);

  // Sets an asynchronous |callback| to be invoked just prior to reporting,
  // allowing users to asynchronously gather and provide additional custom
  // metrics. |callback| will receive the list of metrics when they are ready.
  // Reporting is paused until |callback| is fulfilled.
  // If used, then this method must be called before calling Start().
  void SetReportAdditionalMetricsCallback(
      ReportAdditionalMetricsCallback callback);

  // Sets a |callback| which is invoked to warn that the connection to the
  // remote MetricsRecorder will be terminated. The completion closure passed to
  // |callback| should be invoked to signal flush completion.
  void SetNotifyFlushCallback(NotifyFlushCallback callback);

  // Use when caller needs an explicit flush and then disconnect, such as before
  // termination. Caller will be notified when all events in the buffer are
  // sent.
  void FlushAndDisconnect(base::OnceClosure on_flush_complete);

 private:
  void ConnectFromComponentContext();
  void SetMetricsRecorderInternal(
      fidl::InterfaceHandle<fuchsia::legacymetrics::MetricsRecorder>
          metrics_recorder);
  void ScheduleNextReport();
  void StartReport();
  void Report(std::vector<fuchsia::legacymetrics::Event> additional_metrics);
  void OnMetricsRecorderDisconnected(zx_status_t status);
  void ReconnectMetricsRecorder();
  void OnCloseSoon();
  void CompleteFlush();
  void ResetMetricsRecorderState();

  // Incrementally sends the contents of |to_send_| to |metrics_recorder_|.
  void DrainBuffer();

  base::TimeDelta reconnect_delay_ = kInitialReconnectDelay;
  base::TimeDelta report_interval_;
  ReportAdditionalMetricsCallback report_additional_callback_;
  NotifyFlushCallback notify_flush_callback_;
  bool is_flushing_ = false;
  bool record_ack_pending_ = false;
  std::vector<fuchsia::legacymetrics::Event> to_send_;
  std::unique_ptr<LegacyMetricsUserActionRecorder> user_events_recorder_;

  bool auto_connect_ = true;
  base::RetainingOneShotTimer reconnect_timer_;

  fuchsia::legacymetrics::MetricsRecorderPtr metrics_recorder_;
  base::RetainingOneShotTimer report_timer_;
  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<base::OnceClosure> on_flush_complete_closures_;

  // Prevents use-after-free if |report_additional_callback_| is invoked after
  // |this| is destroyed.
  base::WeakPtrFactory<LegacyMetricsClient> weak_factory_{this};
};

}  // namespace fuchsia_legacymetrics

#endif  // COMPONENTS_FUCHSIA_LEGACYMETRICS_LEGACYMETRICS_CLIENT_H_
