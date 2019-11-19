// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_provider.h"

#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

MetricsProvider::MetricsProvider() {
}

MetricsProvider::~MetricsProvider() {
}

void MetricsProvider::Init() {
}

void MetricsProvider::AsyncInit(const base::Closure& done_callback) {
  done_callback.Run();
}

void MetricsProvider::OnDidCreateMetricsLog() {
}

void MetricsProvider::OnRecordingEnabled() {
}

void MetricsProvider::OnRecordingDisabled() {
}

void MetricsProvider::OnAppEnterBackground() {
}

bool MetricsProvider::HasIndependentMetrics() {
  return false;
}

void MetricsProvider::ProvideIndependentMetrics(
    base::OnceCallback<void(bool)> done_callback,
    ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager) {
  // Either the method HasIndependentMetrics() has been overridden and this
  // method has not, or this method being called without regard to Has().
  // Both are wrong.
  NOTREACHED();
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
}

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
