// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics_services_manager/metrics_services_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"
#include "components/ukm/ukm_service.h"
#include "components/variations/service/variations_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/metrics/structured/neutrino_logging.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace metrics_services_manager {

MetricsServicesManager::MetricsServicesManager(
    std::unique_ptr<MetricsServicesManagerClient> client)
    : client_(std::move(client)),
      may_upload_(false),
      may_record_(false),
      consent_given_(false) {
  DCHECK(client_);
}

MetricsServicesManager::~MetricsServicesManager() {}

void MetricsServicesManager::InstantiateFieldTrialList() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::structured::NeutrinoDevicesLog(
      metrics::structured::NeutrinoDevicesLocation::kCreateEntropyProvider);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  client_->GetMetricsStateManager()->InstantiateFieldTrialList();
}

metrics::MetricsService* MetricsServicesManager::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServiceClient()->GetMetricsService();
}

ukm::UkmService* MetricsServicesManager::GetUkmService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServiceClient()->GetUkmService();
}

variations::VariationsService* MetricsServicesManager::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!variations_service_)
    variations_service_ = client_->CreateVariationsService();
  return variations_service_.get();
}

void MetricsServicesManager::LoadingStateChanged(bool is_loading) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetMetricsServiceClient()->LoadingStateChanged(is_loading);
}

std::unique_ptr<const variations::EntropyProviders>
MetricsServicesManager::CreateEntropyProvidersForTesting() {
  return client_->GetMetricsStateManager()->CreateEntropyProviders();
}

metrics::MetricsServiceClient*
MetricsServicesManager::GetMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_service_client_) {
    metrics_service_client_ = client_->CreateMetricsServiceClient();
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
  // If the user has opted out of metrics, delete local UKM state. We only check
  // consent for UKM.
  if (consent_given_ && !current_consent_given) {
    ukm::UkmService* ukm = GetUkmService();
    if (ukm) {
      ukm->Purge();
      ukm->ResetClientState(ukm::ResetReason::kUpdatePermissions);
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

void MetricsServicesManager::UpdateUploadPermissions(bool may_upload) {
  if (metrics_service_client_->IsMetricsReportingForceEnabled()) {
    UpdatePermissions(true, true, true);
    return;
  }

  UpdatePermissions(client_->IsMetricsReportingEnabled(),
                    client_->IsMetricsConsentGiven(), may_upload);
}

bool MetricsServicesManager::IsMetricsReportingEnabled() const {
  return client_->IsMetricsReportingEnabled();
}

bool MetricsServicesManager::IsMetricsConsentGiven() const {
  return client_->IsMetricsConsentGiven();
}

bool MetricsServicesManager::IsUkmAllowedForAllProfiles() {
  return metrics_service_client_->IsUkmAllowedForAllProfiles();
}

}  // namespace metrics_services_manager
