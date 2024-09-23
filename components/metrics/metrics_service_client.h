// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_CLIENT_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "url/gurl.h"

namespace ukm {
class UkmService;
}

namespace network_time {
class NetworkTimeTracker;
}

namespace variations {
class SyntheticTrialRegistry;
}

class IdentifiabilityStudyState;

namespace metrics {

class MetricsLogUploader;
class MetricsService;

namespace structured {
class StructuredMetricsService;
}

// An abstraction of operations that depend on the embedder's (e.g. Chrome)
// environment.
class MetricsServiceClient {
 public:
  MetricsServiceClient();

  MetricsServiceClient(const MetricsServiceClient&) = delete;
  MetricsServiceClient& operator=(const MetricsServiceClient&) = delete;

  virtual ~MetricsServiceClient();

  // Returns the synthetic trial registry shared by MetricsService and
  // UkmService.
  virtual variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() = 0;

  // Returns the MetricsService instance that this client is associated with.
  // With the exception of testing contexts, the returned instance must be valid
  // for the lifetime of this object (typically, the embedder's client
  // implementation will own the MetricsService instance being returned).
  virtual MetricsService* GetMetricsService() = 0;

  // Returns the UkmService instance that this client is associated with.
  virtual ukm::UkmService* GetUkmService();

  // Returns the IdentifiabilityStudyState instance that this client is
  // associated with. Might be nullptr.
  virtual IdentifiabilityStudyState* GetIdentifiabilityStudyState();

  // Returns the StructuredMetricsService instance that this client is
  // associated with.
  virtual structured::StructuredMetricsService* GetStructuredMetricsService();

  // Returns true if metrics should be uploaded for the given |user_id|, which
  // corresponds to the |user_id| field in ChromeUserMetricsExtension.
  virtual bool ShouldUploadMetricsForUserId(uint64_t user_id);

  // Registers the client id with other services (e.g. crash reporting), called
  // when metrics recording gets enabled.
  virtual void SetMetricsClientId(const std::string& client_id) = 0;

  // Returns the product value to use in uploaded reports, which will be used to
  // set the ChromeUserMetricsExtension.product field. See comments on that
  // field on why it's an int32_t rather than an enum.
  virtual int32_t GetProduct() = 0;

  // Returns the current application locale (e.g. "en-US").
  virtual std::string GetApplicationLocale() = 0;

  // Return a NetworkTimeTracker for access to a server-provided clock.
  virtual const network_time::NetworkTimeTracker* GetNetworkTimeTracker() = 0;

  // Retrieves the brand code string associated with the install, returning
  // false if no brand code is available.
  virtual bool GetBrand(std::string* brand_code) = 0;

  // Returns the release channel (e.g. stable, beta, etc) of the application.
  virtual SystemProfileProto::Channel GetChannel() = 0;

  // Returns true if the application is on the extended stable channel.
  virtual bool IsExtendedStableChannel() = 0;

  // Returns the version of the application as a string.
  virtual std::string GetVersionString() = 0;

  // Called by the metrics service when a new environment has been recorded.
  // Takes the serialized environment as a parameter. The contents of
  // |serialized_environment| are consumed by the call, but the caller maintains
  // ownership.
  virtual void OnEnvironmentUpdate(std::string* serialized_environment) {}

  // Collects child process histograms and merges them into StatisticsRecorder.
  // Called when child process histograms need to be merged ASAP. For example,
  // on Android, when the browser was backgrounded.
  virtual void MergeSubprocessHistograms() {}

  // Called prior to a metrics log being closed, allowing the client to collect
  // extra histograms that will go in that log. Asynchronous API - the client
  // implementation should call |done_callback| when complete.
  virtual void CollectFinalMetricsForLog(base::OnceClosure done_callback) = 0;

  // Get the URL of the metrics server.
  virtual GURL GetMetricsServerUrl();

  // Get the fallback HTTP URL of the metrics server.
  virtual GURL GetInsecureMetricsServerUrl();

  // Creates a MetricsLogUploader with the specified parameters (see comments on
  // MetricsLogUploader for details).
  virtual std::unique_ptr<MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      std::string_view mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const MetricsLogUploader::UploadCallback& on_upload_complete) = 0;

  // Returns the interval between upload attempts. Checks if debugging flags
  // have been set, otherwise defaults to GetStandardUploadInterval().
  base::TimeDelta GetUploadInterval();

  // Returns the standard interval between upload attempts.
  virtual base::TimeDelta GetStandardUploadInterval() = 0;

  // Whether or not the MetricsService should start up quickly and upload the
  // initial report quickly. By default, this work may be delayed by some
  // amount. Only the default behavior should be used in production, but clients
  // can override this in tests if tests need to make assertions on the log
  // data.
  virtual bool ShouldStartUpFastForTesting() const;

  // Called when loading state changed, e.g. start/stop loading.
  virtual void LoadingStateChanged(bool is_loading) {}

  // Returns whether metrics reporting is managed by policy.
  virtual bool IsReportingPolicyManaged();

  // Gets information about the default value for the metrics reporting checkbox
  // shown during first-run.
  virtual EnableMetricsDefault GetMetricsReportingDefaultState();

  // Return true iff the system is currently on a cellular connection.
  virtual bool IsOnCellularConnection();

  // Returns true iff UKM is allowed for all profiles.
  // See //components/ukm/observers/ukm_consent_state_observer.h for details.
  virtual bool IsUkmAllowedForAllProfiles();

  // Returns whether UKM notification listeners were attached to all profiles.
  virtual bool AreNotificationListenersEnabledOnAllProfiles();

  // Gets the app package name (as defined by the embedder). Since package name
  // is only meaningful for Android, other platforms should return the empty
  // string (this is the same as the default behavior). If the package name
  // should not be logged for privacy/fingerprintability reasons, the embedder
  // should return the empty string.
  virtual std::string GetAppPackageNameIfLoggable();

  // Gets the key used to sign metrics uploads. This will be used to compute an
  // HMAC-SHA256 signature of an uploaded log.
  virtual std::string GetUploadSigningKey();

  // Checks if the cloned install detector says that client ids should be reset.
  virtual bool ShouldResetClientIdsOnClonedInstall();

  virtual base::CallbackListSubscription AddOnClonedInstallDetectedCallback(
      base::OnceClosure callback);

  // Specifies local log storage requirements and restrictions.
  virtual MetricsLogStore::StorageLimits GetStorageLimits() const;

  // Sets the callback to run MetricsServiceManager::UpdateRunningServices.
  void SetUpdateRunningServicesCallback(const base::RepeatingClosure& callback);

  // Notify MetricsServiceManager to UpdateRunningServices using callback.
  void UpdateRunningServices();

  // Checks if the user has forced metrics collection on via the override flag.
  bool IsMetricsReportingForceEnabled() const;

  // Initializes per-user metrics collection. For more details what per-user
  // metrics collection is, refer to MetricsService::InitPerUserMetrics.
  //
  // Since the concept of a user is only applicable in Ash Chrome, this function
  // should no-op for other platforms.
  virtual void InitPerUserMetrics() {}

  // Updates the current user's metrics consent. This allows embedders to update
  // the user consent. If there is no current user, then this function will
  // no-op.
  //
  // Since the concept of a user is only applicable on Ash Chrome, this function
  // should no-op for other platforms.
  virtual void UpdateCurrentUserMetricsConsent(bool user_metrics_consent) {}

  // Returns the current user metrics consent if it should be applied to decide
  // the current metrics reporting state. This allows embedders to determine
  // when a user metric consent state should not be applied (ie no logged in
  // user or managed policy).
  //
  // Will return std::nullopt if there is no current user or current user
  // metrics consent should not be applied to determine metrics reporting state.
  //
  // Not all platforms support per-user consent. If per-user consent is not
  // supported, this function should return std::nullopt.
  virtual std::optional<bool> GetCurrentUserMetricsConsent() const;

  // Returns the current user id.
  //
  // Will return std::nullopt if there is no current user, metrics reporting is
  // disabled, or current user should not have a user id.
  //
  // Not all platforms support per-user consent. If per-user consent is not
  // supported, this function should return std::nullopt.
  virtual std::optional<std::string> GetCurrentUserId() const;

 private:
  base::RepeatingClosure update_running_services_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_CLIENT_H_
