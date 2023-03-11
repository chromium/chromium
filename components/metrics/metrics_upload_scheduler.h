// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_UPLOAD_SCHEDULER_H_
#define COMPONENTS_METRICS_METRICS_UPLOAD_SCHEDULER_H_

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/metrics/metrics_scheduler.h"

namespace metrics {

// Scheduler task to drive a ReportingService object's uploading.
class MetricsUploadScheduler : public MetricsScheduler {
 public:
  // Creates MetricsUploadScheduler object with the given |upload_callback|
  // callback to call when uploading should happen.  The callback must
  // arrange to call either UploadFinished or UploadCancelled on completion.
  MetricsUploadScheduler(const base::RepeatingClosure& upload_callback,
                         bool fast_startup_for_testing);

  MetricsUploadScheduler(const MetricsUploadScheduler&) = delete;
  MetricsUploadScheduler& operator=(const MetricsUploadScheduler&) = delete;

  ~MetricsUploadScheduler() override;

  // Callback from MetricsService when a triggered upload finishes.
  void UploadFinished(bool server_is_healthy);

  // Callback from MetricsService when an upload is cancelled.
  // Also stops the scheduler.
  void StopAndUploadCancelled();

  // Callback from MetricsService when an upload is cancelled because it would
  // be over the allowed data usage cap.
  void UploadOverDataUsageCap();

  // Time delay after a log is uploaded successfully before attempting another.
  // On mobile, keeping the radio on is very expensive, so prefer to keep this
  // short and send in bursts.
  static base::TimeDelta GetUnsentLogsInterval();

  // Initial time delay after a log uploaded fails before retrying it.
  static base::TimeDelta GetInitialBackoffInterval();

 private:
  // Time to wait between uploads on success.
  const base::TimeDelta unsent_logs_interval_;

  // Initial time to wait between upload retry attempts.
  const base::TimeDelta initial_backoff_interval_;

  // Time to wait for the next upload attempt if the next one fails.
  base::TimeDelta backoff_interval_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_UPLOAD_SCHEDULER_H_
