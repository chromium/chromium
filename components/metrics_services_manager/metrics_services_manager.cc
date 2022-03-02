// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics_services_manager/metrics_services_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
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

void MetricsServicesManager::InstantiateFieldTrialList(
    const char* enable_gpu_benchmarking_switch) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::structured::NeutrinoDevicesLog(
      metrics::structured::NeutrinoDevicesLocation::kCreateEntropyProvider);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  client_->GetMetricsStateManager()->InstantiateFieldTrialList(
      enable_gpu_benchmarking_switch, metrics::EntropyProviderType::kDefault);
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

std::unique_ptr<const base::FieldTrial::EntropyProvider>
MetricsServicesManager::CreateEntropyProviderForTesting() {
  return client_->GetMetricsStateManager()->CreateDefaultEntropyProvider();
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
    // TODO(skare): revise this - merged in a big change
    ukm->EnableRecording(
        metrics_service_client_->IsUkmAllowedWithExtensionsForAllProfiles());
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

}  // namespace metrics_services_manager
