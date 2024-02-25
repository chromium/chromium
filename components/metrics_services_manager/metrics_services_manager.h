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
}  // namespace metrics

namespace metrics::structured {
class StructuredMetricsService;
}

namespace ukm {
class UkmService;
}

namespace variations {
class EntropyProviders;
class SyntheticTrialRegistry;
class VariationsService;
}  // namespace variations

class IdentifiabilityStudyState;

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

  // Returns the IdentifiabilityStudyState, if it has been created, and nullptr
  // otherwise.
  IdentifiabilityStudyState* GetIdentifiabilityStudyState();

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
  void UpdateUploadPermissions(bool may_upload);

  // Gets the current state of metric reporting.
  bool IsMetricsReportingEnabled() const;

  // Gets the current state of metrics consent.
  bool IsMetricsConsentGiven() const;

  // Returns true iff UKM is allowed for all profiles.
  bool IsUkmAllowedForAllProfiles();

  // Returns a low entropy provider.
  std::unique_ptr<const variations::EntropyProviders>
  CreateEntropyProvidersForTesting();

 private:
  // Returns the MetricsServiceClient, creating it if it hasn't been
  // created yet (and additionally creating the MetricsService in that case).
  metrics::MetricsServiceClient* GetMetricsServiceClient();

  // Updates which services are running to match current permissions.
  void UpdateRunningServices();

  // Updates the state of UkmService to match current permissions.
  void UpdateUkmService();

  // Updates the state of StructuredMetricsService to match current permissions.
  void UpdateStructuredMetricsService();

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
  bool may_upload_;

  // The current metrics recording setting.
  bool may_record_;

  // The current metrics setting reflecting if consent was given.
  bool consent_given_;

  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;

  // The MetricsServiceClient. Owns the MetricsService.
  std::unique_ptr<metrics::MetricsServiceClient> metrics_service_client_;

  // The VariationsService, for server-side experiments infrastructure.
  std::unique_ptr<variations::VariationsService> variations_service_;

  base::WeakPtrFactory<MetricsServicesManager> weak_ptr_factory_{this};
};

}  // namespace metrics_services_manager

#endif  // COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_
