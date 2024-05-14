// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_provider.h"

#include "base/notreached.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

MetricsProvider::MetricsProvider() {
}

MetricsProvider::~MetricsProvider() {
}

void MetricsProvider::Init() {
}

void MetricsProvider::AsyncInit(base::OnceClosure done_callback) {
  std::move(done_callback).Run();
}

bool MetricsProvider::ProvideHistograms() {
  return true;
}

void MetricsProvider::OnDidCreateMetricsLog() {
  emitted_ = ProvideHistograms();
}

void MetricsProvider::OnRecordingEnabled() {
}

void MetricsProvider::OnRecordingDisabled() {
}

void MetricsProvider::OnClientStateCleared() {}

void MetricsProvider::OnAppEnterBackground() {
}

void MetricsProvider::OnPageLoadStarted() {}

bool MetricsProvider::HasIndependentMetrics() {
  return false;
}

void MetricsProvider::ProvideIndependentMetrics(
    base::OnceClosure serialize_log_callback,
    base::OnceCallback<void(bool)> done_callback,
    ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager) {
  // Either the method HasIndependentMetrics() has been overridden and this
  // method has not, or this method being called without regard to Has().
  // Both are wrong.
  NOTREACHED_IN_MIGRATION();
}

void MetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile_proto) {}

void MetricsProvider::ProvideSystemProfileMetricsWithLogCreationTime(
    base::TimeTicks log_creation_time,
    SystemProfileProto* system_profile_proto) {
  ProvideSystemProfileMetrics(system_profile_proto);
}

bool MetricsProvider::HasPreviousSessionData() {
  return false;
}

void MetricsProvider::ProvidePreviousSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  ProvideStabilityMetrics(uma_proto->mutable_system_profile());
}

void MetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  ProvideStabilityMetrics(uma_proto->mutable_system_profile());

  if (!emitted_) {
    ProvideHistograms();
  }
}

void MetricsProvider::ProvideCurrentSessionUKMData() {}

void MetricsProvider::ProvideStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
}

void MetricsProvider::ClearSavedStabilityMetrics() {
}

void MetricsProvider::RecordHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
}

void MetricsProvider::RecordInitialHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
}

}  // namespace metrics
