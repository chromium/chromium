// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_METRICS_PROVIDER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"

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

  MetricsProvider(const MetricsProvider&) = delete;
  MetricsProvider& operator=(const MetricsProvider&) = delete;

  virtual ~MetricsProvider();

  // Called after initialization of MetricsService and field trials.
  virtual void Init();

  // Called during service initialization to allow the provider to start any
  // async initialization tasks.  The service will wait for the provider to
  // call |done_callback| before generating logs for the current session.
  // |done_callback| must be run on the same thread that calls |AsyncInit|.
  virtual void AsyncInit(base::OnceClosure done_callback);

  // Called by OnDidCreateMetricsLog() to provide histograms. If histograms
  // are not emitted successfully, it will be called in
  // ProvideCurrentSessionData().
  // Returns whether or not histograms are emitted successfully.
  // Only override this function if:
  // 1. You want your histograms to be included in every record uploaded to the
  // server.
  // 2. You will not override ProvideCurrentSessionData(),
  // OnDidCreateMetricsLog(), or ProvideStabilityMetrics().
  // TODO(crbug.com/40899764): Refactor the code to remove requirement 2.
  virtual bool ProvideHistograms();

  // Called when a new MetricsLog is created.
  virtual void OnDidCreateMetricsLog();

  // Called when metrics recording has been enabled.
  virtual void OnRecordingEnabled();

  // Called when metrics recording has been disabled.
  virtual void OnRecordingDisabled();

  // Called when metrics client identifiers have been reset.
  //
  // Metrics providers should clean up any persisted state that could be used to
  // associate the previous identifier with the new one.
  //
  // Currently this method is only invoked in UKM.
  virtual void OnClientStateCleared();

  // Called when the application is going into background mode, on platforms
  // where applications may be killed when going into the background (Android,
  // iOS). Providers that buffer histogram data in memory should persist
  // histograms in this callback, as the application may be killed without
  // further notification after this callback.
  virtual void OnAppEnterBackground();

  // Called when a document first starts loading.
  virtual void OnPageLoadStarted();

  // Returns whether there are "independent" metrics that can be retrieved
  // with a call to ProvideIndependentMetrics().
  virtual bool HasIndependentMetrics();

  // Provides a complete and independent uma proto + metrics for uploading.
  // Called once every time HasIndependentMetrics() returns true. The passed in
  // |uma_proto| is by default filled with current session id and core system
  // profile information. This function is called on main thread, but the
  // provider can do async work to fill in |uma_proto| and run |done_callback|
  // on calling thread when complete. Calling |serialize_log_callback| will
  // serialize |uma_proto| so that it is primed to be sent. As an optimization,
  // the provider should call this on a background thread before posting back
  // |done_callback| on the calling thread. However, it is fine not to call this
  // if the thread hopping could introduce data loss (e.g., since the user may
  // shut down the browser before |done_callback| is called). In this case,
  // |done_callback| will "manually" call it synchronously. Ownership of the
  // passed objects remains with the caller and those objects will live until
  // the callback is executed.
  virtual void ProvideIndependentMetrics(
      base::OnceClosure serialize_log_callback,
      base::OnceCallback<void(bool)> done_callback,
      ChromeUserMetricsExtension* uma_proto,
      base::HistogramSnapshotManager* snapshot_manager);

  // Provides additional metrics into the system profile. This is a convenience
  // method over ProvideSystemProfileMetricsWithLogCreationTime() without the
  // |log_creation_time| param. Should not be called directly by services.
  // Do not log histograms within this function; they will not necessarily be
  // added to the UMA record that this system profile is part of.
  virtual void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto);

  // Provides additional metrics into the system profile. The log creation
  // time param provides a timestamp of when the log was opened, which is needed
  // for some metrics providers.
  // Do not log histograms within this function; they will not necessarily be
  // added to the UMA record that this system profile is part of.
  virtual void ProvideSystemProfileMetricsWithLogCreationTime(
      base::TimeTicks log_creation_time,
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

  // Called when building a UKM log about the current session. UKM-specific data
  // should generally only be emitted through this method, and UMA data should
  // be emitted through ProvideCurrentSessionData().
  virtual void ProvideCurrentSessionUKMData();

  // Provides additional stability metrics. Stability metrics can be provided
  // directly into |stability_proto| fields or by logging stability histograms
  // via the UMA_STABILITY_HISTOGRAM_ENUMERATION() macro.
  virtual void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto);

  // Called to indicate that saved stability prefs should be cleared, e.g.
  // because they are from an old version and should not be kept.
  virtual void ClearSavedStabilityMetrics();

  // Called during regular collection to explicitly load histogram snapshots
  // using a snapshot manager. Calls to only PrepareDelta(), not PrepareDeltas()
  // (plural), should be made.
  virtual void RecordHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager);

  // Called during collection of initial metrics to explicitly load histogram
  // snapshots using a snapshot manager. Calls to only PrepareDelta(), not
  // PrepareDeltas() (plural), should be made.
  virtual void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager);

 protected:
  // Used to indicate whether ProvideHistograms() successfully emits histograms
  // when called in OnDidCreateMetricsLog().
  bool emitted_ = false;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_PROVIDER_H_
