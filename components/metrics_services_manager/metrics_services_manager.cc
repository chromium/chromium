// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics_services_manager/metrics_services_manager.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/structured/structured_metrics_service.h"  // nogncheck
#include "components/metrics_services_manager/metrics_services_manager_client.h"
#include "components/ukm/ukm_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace metrics_services_manager {

MetricsServicesManager::MetricsServicesManager(
    std::unique_ptr<MetricsServicesManagerClient> client)
    : client_(std::move(client)),
      may_upload_(false),
      may_record_(false),
      consent_given_(false) {
  DCHECK(client_);
}

MetricsServicesManager::~MetricsServicesManager() = default;

void MetricsServicesManager::InstantiateFieldTrialList() const {
  client_->GetMetricsStateManager()->InstantiateFieldTrialList();
}

variations::SyntheticTrialRegistry*
MetricsServicesManager::GetSyntheticTrialRegistry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!synthetic_trial_registry_) {
    synthetic_trial_registry_ =
        std::make_unique<variations::SyntheticTrialRegistry>();
  }
  return synthetic_trial_registry_.get();
}

metrics::MetricsService* MetricsServicesManager::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServiceClient()->GetMetricsService();
}

ukm::UkmService* MetricsServicesManager::GetUkmService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServiceClient()->GetUkmService();
}

IdentifiabilityStudyState*
MetricsServicesManager::GetIdentifiabilityStudyState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServiceClient()->GetIdentifiabilityStudyState();
}

metrics::structured::StructuredMetricsService*
MetricsServicesManager::GetStructuredMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServiceClient()->GetStructuredMetricsService();
}

variations::VariationsService* MetricsServicesManager::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!variations_service_) {
    variations_service_ =
        client_->CreateVariationsService(GetSyntheticTrialRegistry());
  }
  return variations_service_.get();
}

MetricsServicesManager::OnDidStartLoadingCb
MetricsServicesManager::GetOnDidStartLoadingCb() {
  return base::BindRepeating(&MetricsServicesManager::LoadingStateChanged,
                             weak_ptr_factory_.GetWeakPtr(),
                             /*is_loading=*/true);
}

MetricsServicesManager::OnDidStopLoadingCb
MetricsServicesManager::GetOnDidStopLoadingCb() {
  return base::BindRepeating(&MetricsServicesManager::LoadingStateChanged,
                             weak_ptr_factory_.GetWeakPtr(),
                             /*is_loading=*/false);
}

MetricsServicesManager::OnRendererUnresponsiveCb
MetricsServicesManager::GetOnRendererUnresponsiveCb() {
  return base::BindRepeating(&MetricsServicesManager::OnRendererUnresponsive,
                             weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<const variations::EntropyProviders>
MetricsServicesManager::CreateEntropyProvidersForTesting() {
  // Setting enable_limited_entropy_mode=true to maximize code coverage.
  return client_->GetMetricsStateManager()->CreateEntropyProviders(
      /*enable_limited_entropy_mode=*/true);
}

metrics::MetricsServiceClient*
MetricsServicesManager::GetMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_service_client_) {
    metrics_service_client_ =
        client_->CreateMetricsServiceClient(GetSyntheticTrialRegistry());
    // base::Unretained is safe since |this| owns the metrics_service_client_.
    metrics_service_client_->SetUpdateRunningServicesCallback(
        base::BindRepeating(&MetricsServicesManager::UpdateRunningServices,
                            base::Unretained(this)));
  }
  return metrics_service_client_.get();
}

void MetricsServicesManager::UpdatePermissions(bool current_may_record,
                                               bool current_consent_given,
                                               bool current_may_upload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If the user has opted out of metrics, delete local UKM state.
  // TODO(crbug.com/40267999): Investigate if UMA needs purging logic.
  if (consent_given_ && !current_consent_given) {
    ukm::UkmService* ukm = GetUkmService();
    if (ukm) {
      ukm->Purge();
      ukm->ResetClientState(ukm::ResetReason::kUpdatePermissions);
    }
  }

  // If the user has opted out of metrics, purge Structured Metrics if consent
  // is not granted. On ChromeOS, SM will record specific events when consent is
  // unknown during primarily OOBE; but these events need to be purged once
  // consent is confirmed. This feature shouldn't be used on other platforms.
  if (!current_consent_given) {
    metrics::structured::StructuredMetricsService* sm_service =
        GetStructuredMetricsService();
    if (sm_service) {
      sm_service->Purge();
    }
  }

  // If metrics reporting goes from not consented to consented, create and
  // persist a client ID (either generate a new one or promote the provisional
  // client ID if this is the first run). This can occur in the following
  // situations:
  // 1. The user enables metrics reporting in the FRE
  // 2. The user enables metrics reporting in settings, crash bubble, etc.
  // 3. On startup, after fetching the enable status from the previous session
  //    (if enabled)
  //
  // ForceClientIdCreation() may be called again later on via
  // MetricsService::EnableRecording(), but in that case,
  // ForceClientIdCreation() will be a no-op (will return early since a client
  // ID will already exist).
  //
  // ForceClientIdCreation() must be called here, otherwise, in cases where the
  // user is sampled out, the passed |current_may_record| will be false, which
  // will result in not calling ForceClientIdCreation() in
  // MetricsService::EnableRecording() later on. This is problematic because
  // in the FRE, if the user consents to metrics reporting, this will cause the
  // provisional client ID to not be promoted/stored as the client ID. In the
  // next run, a different client ID will be generated and stored, which will
  // result in different trial assignments—and the client may even be sampled
  // in at that time.
  if (!consent_given_ && current_consent_given) {
    client_->GetMetricsStateManager()->ForceClientIdCreation();
  }

  // Stash the current permissions so that we can update the services correctly
  // when preferences change.
  may_record_ = current_may_record;
  consent_given_ = current_consent_given;
  may_upload_ = current_may_upload;
  UpdateRunningServices();
}

void MetricsServicesManager::LoadingStateChanged(bool is_loading) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetMetricsServiceClient()->LoadingStateChanged(is_loading);
  if (is_loading) {
    GetMetricsService()->OnPageLoadStarted();
  }
}

void MetricsServicesManager::OnRendererUnresponsive() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetMetricsService()->OnApplicationNotIdle();
}

void MetricsServicesManager::UpdateRunningServices() {
  DCHECK(thread_checker_.CalledOnValidThread());
  metrics::MetricsService* metrics = GetMetricsService();

  if (metrics::IsMetricsRecordingOnlyEnabled()) {
    metrics->StartRecordingForTests();
    return;
  }

  client_->UpdateRunningServices(may_record_, may_upload_);

  if (may_record_) {
    if (!metrics->recording_active())
      metrics->Start();
    if (may_upload_)
      metrics->EnableReporting();
    else
      metrics->DisableReporting();
  } else {
    metrics->Stop();
  }

  UpdateUkmService();
  UpdateStructuredMetricsService();
}

void MetricsServicesManager::UpdateUkmService() {
  ukm::UkmService* ukm = GetUkmService();
  if (!ukm)
    return;

  bool listeners_active =
      metrics_service_client_->AreNotificationListenersEnabledOnAllProfiles();
  bool sync_enabled =
      metrics_service_client_->IsMetricsReportingForceEnabled() ||
      metrics_service_client_->IsUkmAllowedForAllProfiles();
  bool is_incognito = client_->IsOffTheRecordSessionActive();

  if (consent_given_ && listeners_active && sync_enabled && !is_incognito) {
    ukm->EnableRecording();
    if (may_upload_)
      ukm->EnableReporting();
    else
      ukm->DisableReporting();
  } else {
    ukm->DisableRecording();
    ukm->DisableReporting();
  }
}

void MetricsServicesManager::UpdateStructuredMetricsService() {
  metrics::structured::StructuredMetricsService* service =
      GetStructuredMetricsService();
  if (!service) {
    return;
  }

  // Maybe write some helper methods for this.
  if (may_record_) {
    service->EnableRecording();
    if (may_upload_) {
      service->EnableReporting();
    } else {
      service->DisableReporting();
    }
  } else {
    service->DisableRecording();
    service->DisableReporting();
  }
}

void MetricsServicesManager::UpdateUploadPermissions(bool may_upload) {
  if (metrics_service_client_->IsMetricsReportingForceEnabled()) {
    UpdatePermissions(/*current_may_record=*/true,
                      /*current_consent_given=*/true,
                      /*current_may_upload=*/true);
    return;
  }

  const auto& enable_state_provider = client_->GetEnabledStateProvider();
  UpdatePermissions(
      /*current_may_record=*/enable_state_provider.IsReportingEnabled(),
      /*current_consent_given=*/enable_state_provider.IsConsentGiven(),
      may_upload);
}

bool MetricsServicesManager::IsMetricsReportingEnabled() const {
  return client_->GetEnabledStateProvider().IsReportingEnabled();
}

bool MetricsServicesManager::IsMetricsConsentGiven() const {
  return client_->GetEnabledStateProvider().IsConsentGiven();
}

bool MetricsServicesManager::IsUkmAllowedForAllProfiles() {
  return metrics_service_client_->IsUkmAllowedForAllProfiles();
}

}  // namespace metrics_services_manager
