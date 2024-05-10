// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_METRICS_ANDROID_METRICS_SERVICE_CLIENT_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_METRICS_ANDROID_METRICS_SERVICE_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/persistent_synthetic_trial_observer.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"

class PrefRegistrySimple;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace metrics {
class MetricsStateManager;

extern const char kCrashpadHistogramAllocatorName[];

// AndroidMetricsServiceClient is a singleton which manages metrics collection
// intended for use by WebView & WebLayer.
//
// Metrics should be enabled iff all these conditions are met:
//  - The user has not opted out.
//  - The app has not opted out.
//  - This client is in the 10% sample (controlled by client ID hash).
// The first two are recorded in |user_consent_| and |app_consent_|, which are
// set by SetHaveMetricsConsent(). The last is recorded in |is_in_sample_|.
//
// Metrics are pseudonymously identified by a randomly-generated "client ID".
// AndroidMetricsServiceClient stores this in prefs, written to the app's data
// directory. There's a different such directory for each user, for each app,
// on each device. So the ID should be unique per (device, app, user) tuple.
//
// In order to be transparent about not associating an ID with an opted out user
// or app, the client ID should only be created and retained when neither the
// user nor the app have opted out. Otherwise, the presence of the ID could give
// the impression that metrics were being collected.
//
// AndroidMetricsServiceClient metrics set up happens like so:
//
//   startup
//      │
//      ├────────────┐
//      │            ▼
//      │         query for consent
//      ▼            │
//   Initialize()    │
//      │            ▼
//      │         SetHaveMetricsConsent()
//      │            │
//      │ ┌──────────┘
//      ▼ ▼
//   MaybeStartMetrics()
//      │
//      ▼
//   MetricsService::Start()
//
// All the named functions in this diagram happen on the UI thread. Querying GMS
// happens in the background, and the result is posted back to the UI thread, to
// SetHaveMetricsConsent(). Querying GMS is slow, so SetHaveMetricsConsent()
// typically happens after Initialize(), but it may happen before.
//
// Each path sets a flag, |init_finished_| or |set_consent_finished_|, to show
// that path has finished, and then calls MaybeStartMetrics(). When
// MaybeStartMetrics() is called the first time, it sees only one flag is true,
// and does nothing. When MaybeStartMetrics() is called the second time, it
// decides whether to start metrics.
//
// If consent was granted, MaybeStartMetrics() determines sampling by hashing
// the client ID (generating a new ID if there was none). If this client is in
// the sample, it then calls MetricsService::Start(). If consent was not
// granted, MaybeStartMetrics() instead clears the client ID, if any.
//
// To match chrome on other platforms (including android), the MetricsService is
// always created.
class AndroidMetricsServiceClient
    : public MetricsServiceClient,
      public EnabledStateProvider,
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver {
 public:
  AndroidMetricsServiceClient();
  ~AndroidMetricsServiceClient() override;

  AndroidMetricsServiceClient(const AndroidMetricsServiceClient&) = delete;
  AndroidMetricsServiceClient& operator=(const AndroidMetricsServiceClient&) =
      delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Initializes, but does not necessarily start, the MetricsService. See the
  // documentation at the top of the file for more details.
  void Initialize(PrefService* pref_service);
  void SetHaveMetricsConsent(bool user_consent, bool app_consent);
  void SetFastStartupForTesting(bool fast_startup_for_testing);
  void SetUploadIntervalForTesting(const base::TimeDelta& upload_interval);

  // Updates the state of whether UKM is enabled or not by calling back into
  // IsUkmAllowedForAllProfiles(). If |must_purge| is true then currently
  // collected data will be purged.
  void UpdateUkm(bool must_purge);

  // Updates the state of the UKM service if it's running. This should be called
  // when a BrowserContext is created or destroyed which would change the value
  // of IsOffTheRecordSessionActive().
  void UpdateUkmService();

  // Whether or not consent state has been determined, regardless of whether
  // it is positive or negative.
  bool IsConsentDetermined() const;

  // EnabledStateProvider
  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;

  // Returns the MetricService only if it has been started (which means consent
  // was given).
  MetricsService* GetMetricsServiceIfStarted();

  // MetricsServiceClient
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() override;
  MetricsService* GetMetricsService() override;
  ukm::UkmService* GetUkmService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  std::string GetApplicationLocale() override;
  const network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  bool GetBrand(std::string* brand_code) override;
  SystemProfileProto::Channel GetChannel() override;
  bool IsExtendedStableChannel() override;
  std::string GetVersionString() override;
  void MergeSubprocessHistograms() override;
  void CollectFinalMetricsForLog(
      const base::OnceClosure done_callback) override;
  std::unique_ptr<MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      std::string_view mime_type,
      MetricsLogUploader::MetricServiceType service_type,
      const MetricsLogUploader::UploadCallback& on_upload_complete) override;
  base::TimeDelta GetStandardUploadInterval() override;
  bool IsUkmAllowedForAllProfiles() override;
  bool ShouldStartUpFastForTesting() const override;

  // Gets the embedding app's package name if it's OK to log. Otherwise, this
  // returns the empty string.
  std::string GetAppPackageNameIfLoggable() override;

  void OnWebContentsCreated(content::WebContents* web_contents);

  // content::RenderProcessHostCreationObserver
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  // Runs |closure| when CollectFinalMetricsForLog() is called, when we begin
  // collecting final metrics.
  void SetCollectFinalMetricsForLogClosureForTesting(base::OnceClosure closure);

  // Runs |listener| after all final metrics have been collected.
  void SetOnFinalMetricsCollectedListenerForTesting(
      base::RepeatingClosure listener);

  metrics::MetricsStateManager* metrics_state_manager() const {
    return metrics_state_manager_.get();
  }

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.metrics
  enum class InstallerPackageType {
    // App has been initially preinstalled in the system image.
    SYSTEM_APP,
    // App has been installed/updated by Google Play Store. Doesn't apply for
    // apps whose most recent updates are sideloaded, even if the app was
    // installed via Google Play Store.
    GOOGLE_PLAY_STORE,
    // App has been Sideloaded or installed/updated through a 3rd party app
    // store.
    OTHER,
  };

  // Returns the embedding application's package name (unconditionally). The
  // value returned by this method shouldn't be logged/stored anywhere, callers
  // should use `GetAppPackageNameIfLoggable`.
  std::string GetAppPackageName();

  // Returns the installer type of the app.
  virtual InstallerPackageType GetInstallerPackageType();

 protected:
  // Called by MaybeStartMetrics() to allow embedder specific initialization.
  virtual void OnMetricsStart() = 0;

  // Called by MaybeStartMetrics() when metrics collection failed to start.
  virtual void OnMetricsNotStarted() = 0;

  // Returns the metrics sampling rate, to be used by IsInSample(). This is a
  // per mille value, so this integer must always be in the inclusive range [0,
  // 1000]. A value of 0 will always be out-of-sample, and a value of 1000 is
  // always in-sample.
  virtual int GetSampleRatePerMille() const = 0;

  // Returns a value in the inclusive range [0, 999], to be compared against a
  // per mille sample rate. This value will be based on a persisted value, so it
  // should be consistent across restarts. This value should also be mostly
  // consistent across upgrades, to avoid significantly impacting IsInSample()
  // and ShouldRecordPackageName(). Virtual for testing.
  virtual int GetSampleBucketValue() const;

  // Determines if the client is within the random sample of clients for which
  // we log metrics. If this returns false, MetricsServiceClient should
  // indicate reporting is disabled. Sampling is due to storage/bandwidth
  // considerations.
  virtual bool IsInSample() const;

  // Determines if the embedder app is the type of app for which we may log the
  // package name. If this returns false, GetAppPackageNameIfLoggable() must
  // return empty string. Virtual for testing.
  virtual bool CanRecordPackageNameForAppType();

  // Determines if this client falls within the group for which the embedding
  // app's package name may be included. If this returns false,
  // GetAppPackageNameIfLoggable() must return the empty string.
  virtual bool ShouldRecordPackageName();

  // Caps the rate at which we include package names in UMA logs, expressed as a
  // per mille value. See GetSampleRatePerMille() for a description of how per
  // mille values are handled.
  virtual int GetPackageNameLimitRatePerMille() = 0;

  // Called by CreateMetricsService, allows the embedder to register additional
  // MetricsProviders. Does nothing by default.
  virtual void RegisterAdditionalMetricsProviders(MetricsService* service);

  // Returns whether there are any OffTheRecord browsers/tabs open.
  virtual bool IsOffTheRecordSessionActive();

  // Returns a URLLoaderFactory when the system uploader isn't used.
  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  void EnsureOnValidSequence() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  PrefService* pref_service() const { return pref_service_; }

 private:
  void MaybeStartMetrics();
  void RegisterForNotifications();

  void RegisterMetricsProvidersAndInitState();
  void CreateUkmService();

  void OnApplicationNotIdle();
  void OnDidStartLoading();

  std::unique_ptr<MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
  // Metrics service observer for synthetic trials.
  metrics::PersistentSyntheticTrialObserver synthetic_trial_observer_;
  base::ScopedObservation<variations::SyntheticTrialRegistry,
                          variations::SyntheticTrialObserver>
      synthetic_trial_observation_{&synthetic_trial_observer_};
  std::unique_ptr<MetricsService> metrics_service_;
  std::unique_ptr<ukm::UkmService> ukm_service_;
  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};
  raw_ptr<PrefService> pref_service_ = nullptr;
  bool init_finished_ = false;
  bool set_consent_finished_ = false;
  bool user_consent_ = false;
  bool app_consent_ = false;
  bool is_client_id_forced_ = false;
  bool fast_startup_for_testing_ = false;
  bool did_start_metrics_ = false;

  // When non-zero, this overrides the default value in
  // GetStandardUploadInterval().
  base::TimeDelta overridden_upload_interval_;

  base::OnceClosure collect_final_metrics_for_log_closure_;
  base::RepeatingClosure on_final_metrics_collected_listener_;

#if DCHECK_IS_ON()
  bool did_start_metrics_with_consent_ = false;
#endif

  // MetricsServiceClient may be created before the UI thread is promoted to
  // BrowserThread::UI. Use |sequence_checker_| to enforce that the
  // MetricsServiceClient is used on a single thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AndroidMetricsServiceClient> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_METRICS_ANDROID_METRICS_SERVICE_CLIENT_H_
