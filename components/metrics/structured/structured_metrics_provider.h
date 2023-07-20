// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/structured_metrics_recorder.h"

namespace metrics::structured {

// StructuredMetricsProvider is responsible for filling out the
// |structured_metrics_event| section of the UMA proto. This class should not be
// instantiated except by the ChromeMetricsServiceClient. This class is not
// thread safe and should only be called on the browser UI sequence, because
// calls from the metrics service come on the UI sequence.
//
// On a call to ProvideCurrentSessionData, the cache of unsent logs is added to
// a ChromeUserMetricsExtension for upload, and is then cleared.
class StructuredMetricsProvider final : public metrics::MetricsProvider {
 public:
  explicit StructuredMetricsProvider(
      base::raw_ptr<StructuredMetricsRecorder> structured_metrics_recorder);
  ~StructuredMetricsProvider() override;
  StructuredMetricsProvider(const StructuredMetricsProvider&) = delete;
  StructuredMetricsProvider& operator=(const StructuredMetricsProvider&) =
      delete;

  StructuredMetricsRecorder& recorder() {
    return *structured_metrics_recorder_;
  }

 private:
  friend class StructuredMetricsProviderTest;
  friend class TestStructuredMetricsProvider;

  // Should only be used for tests.
  //
  // TODO(crbug/1350322): Use this ctor to replace existing ctor.
  StructuredMetricsProvider(
      base::TimeDelta min_independent_metrics_interval,
      base::raw_ptr<StructuredMetricsRecorder> structured_metrics_recorder);

  // Removes all in-memory and on-disk events.
  void Purge();

  // metrics::MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  bool HasIndependentMetrics() override;
  void ProvideIndependentMetrics(base::OnceClosure serialize_log_callback,
                                 base::OnceCallback<void(bool)> done_callback,
                                 ChromeUserMetricsExtension* uma_proto,
                                 base::HistogramSnapshotManager*) override;

  // Tracks the recording state of the StructuredMetricsRecorder.
  bool recording_enabled_ = false;

  // The last time we provided independent metrics.
  base::Time last_provided_independent_metrics_;

  // The minimum waiting time between successive deliveries of independent
  // metrics to the metrics service via ProvideIndependentMetrics. This is set
  // carefully: metrics logs are stored in a queue of limited size, and are
  // uploaded roughly every 30 minutes.
  //
  // If this value is 0, then there will be no waiting time and events will be
  // available on every ProvideIndependentMetrics.
  base::TimeDelta min_independent_metrics_interval_;

  base::raw_ptr<StructuredMetricsRecorder> structured_metrics_recorder_;

  base::WeakPtrFactory<StructuredMetricsProvider> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PROVIDER_H_
