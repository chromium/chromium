// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_METRICS_PROVIDER_H_

#include "base/callback.h"
#include "base/macros.h"

namespace base {
class HistogramSnapshotManager;
}  // namespace base

namespace metrics {

class ChromeUserMetricsExtension;
class SystemProfileProto;

// MetricsProvider is an interface allowing different parts of the UMA protos to
// be filled out by different classes.
class MetricsProvider {
 public:
  MetricsProvider();
  virtual ~MetricsProvider();

  // Called after initialiazation of MetricsService and field trials.
  virtual void Init();

  // Called during service initialization to allow the provider to start any
  // async initialization tasks.  The service will wait for the provider to
  // call |done_callback| before generating logs for the current session.
  virtual void AsyncInit(const base::Closure& done_callback);

  // Called when a new MetricsLog is created.
  virtual void OnDidCreateMetricsLog();

  // Called when metrics recording has been enabled.
  virtual void OnRecordingEnabled();

  // Called when metrics recording has been disabled.
  virtual void OnRecordingDisabled();

  // Called when the application is going into background mode, on platforms
  // where applications may be killed when going into the background (Android,
  // iOS). Providers that buffer histogram data in memory should persist
  // histograms in this callback, as the application may be killed without
  // further notification after this callback.
  virtual void OnAppEnterBackground();

  // Returns whether there are "independent" metrics that can be retrieved
  // with a call to ProvideIndependentMetrics().
  virtual bool HasIndependentMetrics();

  // Provides a complete and independent system profile + metrics for uploading.
  // This may or may not be executed on a background thread and the callback
  // executed when complete. Ownership of the passed objects remains with the
  // caller and those objects must live until the callback is executed.
  virtual void ProvideIndependentMetrics(
      base::OnceCallback<void(bool)> done_callback,
      SystemProfileProto* system_profile_proto,
      base::HistogramSnapshotManager* snapshot_manager);

  // Provides additional metrics into the system profile.
  virtual void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto);

  // Called once at startup to see whether this provider has critical data
  // to provide about the previous session.
  // Returning true will trigger ProvidePreviousSessionData on all other
  // registered metrics providers.
  // Default implementation always returns false.
  virtual bool HasPreviousSessionData();

  // Called when building a log about the previous session, so the provider
  // can provide data about it.  Stability metrics can be provided
  // directly into |stability_proto| fields or by logging stability histograms
  // via the UMA_STABILITY_HISTOGRAM_ENUMERATION() macro.
  virtual void ProvidePreviousSessionData(
      ChromeUserMetricsExtension* uma_proto);

  // Called when building a log about the current session, so the provider
  // can provide data about it.
  virtual void ProvideCurrentSessionData(ChromeUserMetricsExtension* uma_proto);

  // Provides additional stability metrics. Stability metrics can be provided
  // directly into |stability_proto| fields or by logging stability histograms
  // via the UMA_STABILITY_HISTOGRAM_ENUMERATION() macro.
  virtual void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto);

  // Called to indicate that saved stability prefs should be cleared, e.g.
  // because they are from an old version and should not be kept.
  virtual void ClearSavedStabilityMetrics();

  // Called during regular collection to explicitly load histogram snapshots
  // using a snapshot manager. PrepareDeltas() will have already been called
  // and FinishDeltas() will be called later; calls to only PrepareDelta(),
  // not PrepareDeltas (plural), should be made.
  virtual void RecordHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager);

  // Called during collection of initial metrics to explicitly load histogram
  // snapshots using a snapshot manager. PrepareDeltas() will have already
  // been called and FinishDeltas() will be called later; calls to only
  // PrepareDelta(), not PrepareDeltas (plural), should be made.
  virtual void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_PROVIDER_H_
