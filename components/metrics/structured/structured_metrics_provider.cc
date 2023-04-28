// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_provider.h"

#include <sstream>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/current_thread.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics::structured {
namespace {

using ::metrics::ChromeUserMetricsExtension;
using ::metrics::SystemProfileProto;

// This is set carefully: metrics logs are stored in a queue of limited size,
// and are uploaded roughly every 30 minutes.
constexpr base::TimeDelta kMinIndependentMetricsInterval = base::Minutes(45);

}  // namespace

StructuredMetricsProvider::StructuredMetricsProvider(
    base::raw_ptr<metrics::MetricsProvider> system_profile_provider)
    : StructuredMetricsProvider(kMinIndependentMetricsInterval,
                                std::make_unique<StructuredMetricsRecorder>(
                                    system_profile_provider)) {
  DCHECK(system_profile_provider);
}

StructuredMetricsProvider::StructuredMetricsProvider(
    base::TimeDelta min_independent_metrics_interval,
    std::unique_ptr<StructuredMetricsRecorder> structured_metrics_recorder)
    : min_independent_metrics_interval_(min_independent_metrics_interval),
      structured_metrics_recorder_(std::move(structured_metrics_recorder)) {}

StructuredMetricsProvider::~StructuredMetricsProvider() = default;

void StructuredMetricsProvider::Purge() {
  recorder().Purge();
}

void StructuredMetricsProvider::OnRecordingEnabled() {
  recording_enabled_ = true;
  recorder().EnableRecording();
}

void StructuredMetricsProvider::OnRecordingDisabled() {
  recording_enabled_ = false;
  recorder().DisableRecording();
}

void StructuredMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  DCHECK(base::CurrentUIThread::IsSet());
  recorder().ProvideUmaEventMetrics(*uma_proto);
}

bool StructuredMetricsProvider::HasIndependentMetrics() {
  if (!IsIndependentMetricsUploadEnabled()) {
    return false;
  }

  if (!recorder().can_provide_metrics()) {
    return false;
  }

  if (base::Time::Now() - last_provided_independent_metrics_ <
      min_independent_metrics_interval_) {
    return false;
  }

  return recorder().events()->non_uma_events_size() != 0;
}

void StructuredMetricsProvider::ProvideIndependentMetrics(
    base::OnceCallback<void(bool)> done_callback,
    ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager*) {
  DCHECK(base::CurrentUIThread::IsSet());

  if (!recording_enabled_) {
    return;
  }

  last_provided_independent_metrics_ = base::Time::Now();

  recorder().ProvideEventMetrics(*uma_proto);

  // Independent events should not be associated with the client_id, so clear
  // it.
  uma_proto->clear_client_id();
  // TODO(crbug/1052796): Remove the UMA timer code, which is currently used to
  // determine if it is worth to finalize independent logs in the background
  // by measuring the time it takes to execute the callback
  // MetricsService::PrepareProviderMetricsLogDone().
  SCOPED_UMA_HISTOGRAM_TIMER(
      "UMA.IndependentLog.StructuredMetricsProvider.FinalizeTime");
  std::move(done_callback).Run(true);
}

}  // namespace metrics::structured
