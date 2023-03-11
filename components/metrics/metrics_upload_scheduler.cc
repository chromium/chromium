// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_upload_scheduler.h"

#include <stdint.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/metrics/metrics_scheduler.h"

namespace metrics {
namespace {

// When uploading metrics to the server fails, we progressively wait longer and
// longer before sending the next log. This backoff process helps reduce load
// on a server that is having issues.
// The following is the multiplier we use to expand that inter-log duration.
const double kBackoffMultiplier = 2;

// The maximum backoff interval in hours.
const int kMaxBackoffIntervalHours = 24;

// Minutes to wait if we are unable to upload due to data usage cap.
const int kOverDataUsageIntervalMinutes = 5;

// Increases the upload interval each time it's called, to handle the case
// where the server is having issues.
base::TimeDelta BackOffUploadInterval(base::TimeDelta interval) {
  DCHECK_GT(kBackoffMultiplier, 1.0);
  interval = base::Microseconds(
      static_cast<int64_t>(kBackoffMultiplier * interval.InMicroseconds()));

  base::TimeDelta max_interval = base::Hours(kMaxBackoffIntervalHours);
  if (interval > max_interval || interval.InSeconds() < 0) {
    interval = max_interval;
  }
  return interval;
}

}  // namespace

MetricsUploadScheduler::MetricsUploadScheduler(
    const base::RepeatingClosure& upload_callback,
    bool fast_startup_for_testing)
    : MetricsScheduler(upload_callback, fast_startup_for_testing),
      unsent_logs_interval_(GetUnsentLogsInterval()),
      initial_backoff_interval_(GetInitialBackoffInterval()),
      backoff_interval_(initial_backoff_interval_) {}

MetricsUploadScheduler::~MetricsUploadScheduler() = default;

// static
base::TimeDelta MetricsUploadScheduler::GetUnsentLogsInterval() {
  return base::Seconds(3);
}

// static
base::TimeDelta MetricsUploadScheduler::GetInitialBackoffInterval() {
  return base::Minutes(5);
}

void MetricsUploadScheduler::UploadFinished(bool server_is_healthy) {
  // If the server is having issues, back off. Otherwise, reset to default
  // (unless there are more logs to send, in which case the next upload should
  // happen sooner).
  if (!server_is_healthy) {
    TaskDone(backoff_interval_);
    backoff_interval_ = BackOffUploadInterval(backoff_interval_);
  } else {
    backoff_interval_ = initial_backoff_interval_;
    TaskDone(unsent_logs_interval_);
  }
}

void MetricsUploadScheduler::StopAndUploadCancelled() {
  Stop();
  TaskDone(unsent_logs_interval_);
}

void MetricsUploadScheduler::UploadOverDataUsageCap() {
  TaskDone(base::Minutes(kOverDataUsageIntervalMinutes));
}

}  // namespace metrics
