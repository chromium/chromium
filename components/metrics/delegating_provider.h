// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DELEGATING_PROVIDER_H_
#define COMPONENTS_METRICS_DELEGATING_PROVIDER_H_

#include <memory>
#include <vector>

#include "components/metrics/metrics_provider.h"

namespace metrics {

// A MetricsProvider which manages a set of other MetricsProviders.
// Calls to this providers methods are forwarded to all of the registered
// metrics providers, allowing the group to be handled as a single provider.
class DelegatingProvider final : public MetricsProvider {
 public:
  DelegatingProvider();

  DelegatingProvider(const DelegatingProvider&) = delete;
  DelegatingProvider& operator=(const DelegatingProvider&) = delete;

  ~DelegatingProvider() override;

  // Registers an additional MetricsProvider to forward calls to.
  void RegisterMetricsProvider(std::unique_ptr<MetricsProvider> delegate);

  // Gets the list of registered providers.
  const std::vector<std::unique_ptr<MetricsProvider>>& GetProviders();

  // MetricsProvider:
  void Init() override;
  void AsyncInit(base::OnceClosure done_callback) override;
  void OnDidCreateMetricsLog() override;
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void OnClientStateCleared() override;
  void OnAppEnterBackground() override;
  void OnPageLoadStarted() override;
  bool HasIndependentMetrics() override;
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override;
  void ProvideSystemProfileMetricsWithLogCreationTime(
      base::TimeTicks log_creation_time,
      SystemProfileProto* system_profile_proto) override;
  bool HasPreviousSessionData() override;
  void ProvidePreviousSessionData(
      ChromeUserMetricsExtension* uma_proto) override;
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;
  void ProvideCurrentSessionUKMData() override;
  void ClearSavedStabilityMetrics() override;
  void RecordHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) override;
  void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) override;

 private:
  std::vector<std::unique_ptr<MetricsProvider>> metrics_providers_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DELEGATING_PROVIDER_H_
