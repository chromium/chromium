// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_UPLOAD_SCHEDULER_H_
#define COMPONENTS_METRICS_METRICS_UPLOAD_SCHEDULER_H_

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/metrics/metrics_scheduler.h"

namespace metrics {

// Scheduler task to drive a ReportingService object's uploading.
class MetricsUploadScheduler : public MetricsScheduler {
 public:
  // Creates MetricsUploadScheduler object with the given |upload_callback|
  // callback to call when uploading should happen.  The callback must
  // arrange to call either UploadFinished or UploadCancelled on completion.
  MetricsUploadScheduler(const base::Closure& upload_callback,
                         bool fast_startup_for_testing);
  ~MetricsUploadScheduler() override;

  // Callback from MetricsService when a triggered upload finishes.
  void UploadFinished(bool server_is_healthy);

  // Callback from MetricsService when an upload is cancelled.
  // Also stops the scheduler.
  void StopAndUploadCancelled();

  // Callback from MetricsService when an upload is cancelled because it would
  // be over the allowed data usage cap.
  void UploadOverDataUsageCap();

 private:
  // Time to wait between uploads on success.
  const base::TimeDelta unsent_logs_interval_;

  // Initial time to wait between upload retry attempts.
  const base::TimeDelta initial_backoff_interval_;

  // Time to wait for the next upload attempt if the next one fails.
  base::TimeDelta backoff_interval_;

  DISALLOW_COPY_AND_ASSIGN(MetricsUploadScheduler);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_UPLOAD_SCHEDULER_H_
