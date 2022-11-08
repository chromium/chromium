// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_TEST_TEST_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_TEST_TEST_METRICS_PROVIDER_H_

#include "base/metrics/histogram_snapshot_manager.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {

// A simple implementation of MetricsProvider that checks that its providing
// functions are called, for use in tests.
class TestMetricsProvider : public MetricsProvider {
 public:
  TestMetricsProvider() = default;

  TestMetricsProvider(const TestMetricsProvider&) = delete;
  TestMetricsProvider& operator=(const TestMetricsProvider&) = delete;

  // MetricsProvider:
  void Init() override;
  void OnRecordingDisabled() override;
  bool HasPreviousSessionData() override;
  void ProvidePreviousSessionData(
      ChromeUserMetricsExtension* uma_proto) override;
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;
  void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) override;
  void RecordHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) override;

  bool init_called() { return init_called_; }
  bool on_recording_disabled_called() { return on_recording_disabled_called_; }
  bool has_initial_stability_metrics_called() {
    return has_initial_stability_metrics_called_;
  }
  void set_has_initial_stability_metrics(bool has_initial_stability_metrics) {
    has_initial_stability_metrics_ = has_initial_stability_metrics;
  }
  bool provide_initial_stability_metrics_called() const {
    return provide_initial_stability_metrics_called_;
  }
  bool provide_stability_metrics_called() const {
    return provide_stability_metrics_called_;
  }
  bool provide_system_profile_metrics_called() const {
    return provide_system_profile_metrics_called_;
  }
  bool record_initial_histogram_snapshots_called() const {
    return record_initial_histogram_snapshots_called_;
  }
  bool record_histogram_snapshots_called() const {
    return record_histogram_snapshots_called_;
  }
  void set_record_histogram_snapshots_called(bool val) {
    record_histogram_snapshots_called_ = val;
  }

 private:
  bool init_called_ = false;
  bool on_recording_disabled_called_ = false;
  bool has_initial_stability_metrics_ = false;
  bool has_initial_stability_metrics_called_ = false;
  bool provide_initial_stability_metrics_called_ = false;
  bool provide_stability_metrics_called_ = false;
  bool provide_system_profile_metrics_called_ = false;
  bool record_initial_histogram_snapshots_called_ = false;
  bool record_histogram_snapshots_called_ = false;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_TEST_TEST_METRICS_PROVIDER_H_
