// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_
#define COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/threading/thread_checker.h"
#include "components/variations/synthetic_trial_registry.h"

namespace metrics {
class MetricsService;
class MetricsServiceClient;
class ClonedInstallDetector;
}  // namespace metrics

namespace metrics::structured {
class StructuredMetricsService;
}

namespace search_engines {
class SearchEngineChoiceServiceClient;
}

namespace ukm {
class UkmService;
}

namespace metrics::dwa {
class DwaService;
}

namespace metrics::private_metrics {
class PumaService;
}

namespace variations {
class EntropyProviders;
class SyntheticTrialRegistry;
class VariationsService;
}  // namespace variations

namespace metrics_services_manager {

class MetricsServicesManagerClient;

// MetricsServicesManager is a helper class for embedders that use the various
// metrics-related services in a Chrome-like fashion: MetricsService (via its
// client) and VariationsService.
class MetricsServicesManager {
 public:
  using OnDidStartLoadingCb = base::RepeatingClosure;
  using OnDidStopLoadingCb = base::RepeatingClosure;
  using OnRendererUnresponsiveCb = base::RepeatingClosure;

  // Creates the MetricsServicesManager with the given client.
  explicit MetricsServicesManager(
      std::unique_ptr<MetricsServicesManagerClient> client);

  MetricsServicesManager(const MetricsServicesManager&) = delete;
  MetricsServicesManager& operator=(const MetricsServicesManager&) = delete;

  virtual ~MetricsServicesManager();

  // Instantiates the FieldTrialList using Chrome's default entropy provider.
  //
  // Side effect: Initializes the CleanExitBeacon.
  void InstantiateFieldTrialList() const;

  // Returns the SyntheticTrialRegistry, creating it if it hasn't been created
  // yet.
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry();

  // Returns the MetricsService, creating it if it hasn't been created yet (and
  // additionally creating the MetricsServiceClient in that case).
  metrics::MetricsService* GetMetricsService();

  // Returns the UkmService, creating it if it hasn't been created yet.
  ukm::UkmService* GetUkmService();

  // Returns the DwaService, creating it if it hasn't been created yet.
  metrics::dwa::DwaService* GetDwaService();

  // Returns the PumaService, creating it if it hasn't been created yet.
  metrics::private_metrics::PumaService* GetPumaService();

  // Returns the StructuredMetricsService associated with the
  // |metrics_service_client_|.
  metrics::structured::StructuredMetricsService* GetStructuredMetricsService();

  // Returns the VariationsService, creating it if it hasn't been created yet.
  variations::VariationsService* GetVariationsService();

  // Returns an |OnDidStartLoadingCb| callback.
  OnDidStartLoadingCb GetOnDidStartLoadingCb();

  // Returns an |OnDidStopLoadingCb| callback.
  OnDidStopLoadingCb GetOnDidStopLoadingCb();

  // Returns an |OnRendererUnresponsiveCb| callback.
  OnRendererUnresponsiveCb GetOnRendererUnresponsiveCb();

  // Updates the managed services when permissions for uploading metrics change.
  // Note: Normally, uploads will happen when collection is enabled, but the
  // `may_upload` params allows disabling uploads separately from collection
  // (e.g. if network is unavailable).
  void UpdateUploadPermissions(bool may_upload = true);

  // Gets the current state of metric reporting.
  bool IsMetricsReportingEnabled() const;

  // Gets the current state of metrics consent.
  bool IsMetricsConsentGiven() const;

  // Returns true iff UKM is allowed for all profiles.
  bool IsUkmAllowedForAllProfiles();

  // Returns true iff DWA is allowed for all profiles.
  bool IsDwaAllowedForAllProfiles();

  // Returns a low entropy provider.
  std::unique_ptr<const variations::EntropyProviders>
  CreateEntropyProvidersForTesting();

  // Returns the ClonedInstallDetector associated with the `client_`.
  metrics::ClonedInstallDetector* GetClonedInstallDetectorForTesting();

 private:
  friend class search_engines::SearchEngineChoiceServiceClient;

  // Returns the ClonedInstallDetector associated with the `client_`.
  // Marked as private (exposed selectively via friend classes) for the metrics
  // team to be able to control and monitor if/how this function gets called.
  const metrics::ClonedInstallDetector& GetClonedInstallDetector() const;

  // Returns the MetricsServiceClient, creating it if it hasn't been
  // created yet (and additionally creating the MetricsService in that case).
  metrics::MetricsServiceClient* GetMetricsServiceClient();

  // Updates which services are running to match current permissions.
  void UpdateRunningServices();

  // Updates the state of UkmService to match current permissions.
  void UpdateUkmService();

  // Updates the state of StructuredMetricsService to match current permissions.
  void UpdateStructuredMetricsService();

  // Updates the state of DwaService to match current permissions.
  void UpdateDwaService();

  // Updates the state of PumaService to match current permissions.
  void UpdatePumaService();

  // Updates the managed services when permissions for recording/uploading
  // metrics change.
  void UpdatePermissions(bool current_may_record,
                         bool current_consent_given,
                         bool current_may_upload);

  // Called when loading state changed.
  void LoadingStateChanged(bool is_loading);

  // Used by |GetOnRendererUnresponsiveCb| to construct the callback that will
  // be run by |MetricsServicesWebContentsObserver|.
  void OnRendererUnresponsive();

  // The client passed in from the embedder.
  const std::unique_ptr<MetricsServicesManagerClient> client_;

  // Ensures that all functions are called from the same thread.
  base::ThreadChecker thread_checker_;

  // The current metrics reporting setting.
  bool may_upload_ = false;

  // The current metrics recording setting.
  bool may_record_ = false;

  // The current metrics setting for reporting metrics.
  bool consent_given_ = false;

  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;

  // The MetricsServiceClient. Owns the MetricsService.
  std::unique_ptr<metrics::MetricsServiceClient> metrics_service_client_;

  // The VariationsService, for server-side experiments infrastructure.
  std::unique_ptr<variations::VariationsService> variations_service_;

  base::WeakPtrFactory<MetricsServicesManager> weak_ptr_factory_{this};
};

}  // namespace metrics_services_manager

#endif  // COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_
