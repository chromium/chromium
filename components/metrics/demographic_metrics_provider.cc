// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographic_metrics_provider.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace metrics {

// static
const base::Feature DemographicMetricsProvider::kDemographicMetricsReporting = {
    "DemographicMetricsReporting", base::FEATURE_DISABLED_BY_DEFAULT};

DemographicMetricsProvider::DemographicMetricsProvider(
    std::unique_ptr<ProfileClient> profile_client,
    MetricsLogUploader::MetricServiceType metrics_service_type)
    : profile_client_(std::move(profile_client)),
      metrics_service_type_(metrics_service_type) {
  DCHECK(profile_client_);
}

DemographicMetricsProvider::~DemographicMetricsProvider() {}

base::Optional<syncer::UserDemographics>
DemographicMetricsProvider::ProvideSyncedUserNoisedBirthYearAndGender() {
  // Skip if feature disabled.
  if (!base::FeatureList::IsEnabled(kDemographicMetricsReporting))
    return base::nullopt;

  // Skip if not exactly one Profile on disk. Having more than one Profile that
  // is using the browser can make demographics less relevant. This approach
  // cannot determine if there is more than 1 distinct user using the Profile.
  if (profile_client_->GetNumberOfProfilesOnDisk() != 1) {
    LogUserDemographicsStatusInHistogram(
        syncer::UserDemographicsStatus::kMoreThanOneProfile);
    return base::nullopt;
  }

  syncer::SyncService* sync_service = profile_client_->GetSyncService();
  // Skip if no sync service.
  if (!sync_service) {
    LogUserDemographicsStatusInHistogram(
        syncer::UserDemographicsStatus::kNoSyncService);
    return base::nullopt;
  }

  syncer::UserDemographicsResult demographics_result =
      sync_service->GetUserNoisedBirthYearAndGender(
          profile_client_->GetNetworkTime());
  LogUserDemographicsStatusInHistogram(demographics_result.status());

  if (demographics_result.IsSuccess())
    return demographics_result.value();

  return base::nullopt;
}

void DemographicMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  ProvideSyncedUserNoisedBirthYearAndGender(uma_proto);
}

void DemographicMetricsProvider::
    ProvideSyncedUserNoisedBirthYearAndGenderToReport(ukm::Report* report) {
  ProvideSyncedUserNoisedBirthYearAndGender(report);
}

void DemographicMetricsProvider::LogUserDemographicsStatusInHistogram(
    syncer::UserDemographicsStatus status) {
  switch (metrics_service_type_) {
    case MetricsLogUploader::MetricServiceType::UMA:
      base::UmaHistogramEnumeration("UMA.UserDemographics.Status", status);
      return;
    case MetricsLogUploader::MetricServiceType::UKM:
      base::UmaHistogramEnumeration("UKM.UserDemographics.Status", status);
      return;
  }
  NOTREACHED();
}

}  // namespace metrics
