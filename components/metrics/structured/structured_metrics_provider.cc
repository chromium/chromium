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

}  // namespace

StructuredMetricsProvider::StructuredMetricsProvider(
    base::raw_ptr<StructuredMetricsRecorder> structured_metrics_recorder)
    : StructuredMetricsProvider(base::Minutes(GetUploadCadenceMinutes()),
                                structured_metrics_recorder) {}

StructuredMetricsProvider::StructuredMetricsProvider(
    base::TimeDelta min_independent_metrics_interval,
    base::raw_ptr<StructuredMetricsRecorder> structured_metrics_recorder)
    : min_independent_metrics_interval_(min_independent_metrics_interval),
      structured_metrics_recorder_(structured_metrics_recorder) {
  DCHECK(structured_metrics_recorder_);
}

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
  // When StructuredMetricsService is enabled then the StructuredMetricsProvider
  // will not upload metrics.
  if (base::FeatureList::IsEnabled(kEnabledStructuredMetricsService)) {
    return;
  }
  recorder().ProvideUmaEventMetrics(*uma_proto);
}

bool StructuredMetricsProvider::HasIndependentMetrics() {
  // If the StructuredMetricsService is enabled then we should not upload using
  // |this|. When enabled this function will always return false, resulting in
  // ProviderIndependentMetrics never being called.
  if (base::FeatureList::IsEnabled(kEnabledStructuredMetricsService)) {
    return false;
  }

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

  // When StructuredMetricsService is enabled then the StructuredMetricsProvider
  // will not upload metrics.
  if (base::FeatureList::IsEnabled(kEnabledStructuredMetricsService)) {
    NOTREACHED();
    std::move(done_callback).Run(false);
    return;
  }

  if (!recording_enabled_) {
    std::move(done_callback).Run(false);
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
