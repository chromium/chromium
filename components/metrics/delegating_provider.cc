// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/delegating_provider.h"

#include "base/barrier_closure.h"

namespace metrics {

DelegatingProvider::DelegatingProvider() = default;

DelegatingProvider::~DelegatingProvider() = default;

void DelegatingProvider::RegisterMetricsProvider(
    std::unique_ptr<MetricsProvider> provider) {
  metrics_providers_.push_back(std::move(provider));
}

const std::vector<std::unique_ptr<MetricsProvider>>&
DelegatingProvider::GetProviders() {
  return metrics_providers_;
}

void DelegatingProvider::Init() {
  for (auto& provider : metrics_providers_)
    provider->Init();
}

void DelegatingProvider::AsyncInit(const base::Closure& done_callback) {
  base::Closure barrier =
      base::BarrierClosure(metrics_providers_.size(), done_callback);
  for (auto& provider : metrics_providers_) {
    provider->AsyncInit(barrier);
  }
}

void DelegatingProvider::OnDidCreateMetricsLog() {
  for (auto& provider : metrics_providers_)
    provider->OnDidCreateMetricsLog();
}

void DelegatingProvider::OnRecordingEnabled() {
  for (auto& provider : metrics_providers_)
    provider->OnRecordingEnabled();
}

void DelegatingProvider::OnRecordingDisabled() {
  for (auto& provider : metrics_providers_)
    provider->OnRecordingDisabled();
}

void DelegatingProvider::OnAppEnterBackground() {
  for (auto& provider : metrics_providers_)
    provider->OnAppEnterBackground();
}

bool DelegatingProvider::HasIndependentMetrics() {
  // These are collected seperately for each provider.
  NOTREACHED();
  return false;
}

void DelegatingProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {
  // ProvideSystemProfileMetricsWithLogCreationTime() should be called instead.
  NOTREACHED();
}

void DelegatingProvider::ProvideSystemProfileMetricsWithLogCreationTime(
    base::TimeTicks log_creation_time,
    SystemProfileProto* system_profile_proto) {
  for (auto& provider : metrics_providers_) {
    provider->ProvideSystemProfileMetricsWithLogCreationTime(
        log_creation_time, system_profile_proto);
  }
}

bool DelegatingProvider::HasPreviousSessionData() {
  // All providers are queried (rather than stopping after the first "true"
  // response) in case they do any kind of setup work in preparation for
  // the later call to RecordInitialHistogramSnapshots().
  bool has_stability_metrics = false;
  for (auto& provider : metrics_providers_)
    has_stability_metrics |= provider->HasPreviousSessionData();

  return has_stability_metrics;
}

void DelegatingProvider::ProvidePreviousSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  for (const auto& provider : metrics_providers_)
    provider->ProvidePreviousSessionData(uma_proto);
}

void DelegatingProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  for (const auto& provider : metrics_providers_)
    provider->ProvideCurrentSessionData(uma_proto);
}

void DelegatingProvider::ClearSavedStabilityMetrics() {
  for (auto& provider : metrics_providers_)
    provider->ClearSavedStabilityMetrics();
}

void DelegatingProvider::RecordHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
  for (auto& provider : metrics_providers_)
    provider->RecordHistogramSnapshots(snapshot_manager);
}

void DelegatingProvider::RecordInitialHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
  for (auto& provider : metrics_providers_)
    provider->RecordInitialHistogramSnapshots(snapshot_manager);
}

}  // namespace metrics
