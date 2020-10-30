// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographics/demographic_metrics_provider.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "components/sync/driver/sync_service_utils.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace metrics {

namespace {

bool CanUploadDemographicsToGoogle(syncer::SyncService* sync_service) {
  DCHECK(sync_service);

  // Require that the user has opted into sync the feature, without just relying
  // on PRIORITY_PREFERENCES start sync-ing.
  if (!sync_service->IsSyncFeatureEnabled()) {
    return false;
  }

  switch (GetUploadToGoogleState(sync_service, syncer::PRIORITY_PREFERENCES)) {
    case syncer::UploadState::NOT_ACTIVE:
      return false;
    case syncer::UploadState::INITIALIZING:
      // Note that INITIALIZING is considered good enough, because sync is known
      // to be enabled, and transient errors don't really matter here.
    case syncer::UploadState::ACTIVE:
      return true;
  }
}

}  // namespace

// static
const base::Feature DemographicMetricsProvider::kDemographicMetricsReporting = {
    "DemographicMetricsReporting", base::FEATURE_ENABLED_BY_DEFAULT};

DemographicMetricsProvider::DemographicMetricsProvider(
    std::unique_ptr<ProfileClient> profile_client,
    MetricsLogUploader::MetricServiceType metrics_service_type)
    : profile_client_(std::move(profile_client)),
      metrics_service_type_(metrics_service_type) {
  DCHECK(profile_client_);
}

DemographicMetricsProvider::~DemographicMetricsProvider() {}

base::Optional<UserDemographics>
DemographicMetricsProvider::ProvideSyncedUserNoisedBirthYearAndGender() {
  // Skip if feature disabled.
  if (!base::FeatureList::IsEnabled(kDemographicMetricsReporting))
    return base::nullopt;

  // Skip if not exactly one Profile on disk. Having more than one Profile that
  // is using the browser can make demographics less relevant. This approach
  // cannot determine if there is more than 1 distinct user using the Profile.
  if (profile_client_->GetNumberOfProfilesOnDisk() != 1) {
    LogUserDemographicsStatusInHistogram(
        UserDemographicsStatus::kMoreThanOneProfile);
    return base::nullopt;
  }

  syncer::SyncService* sync_service = profile_client_->GetSyncService();
  // Skip if no sync service.
  if (!sync_service) {
    LogUserDemographicsStatusInHistogram(
        UserDemographicsStatus::kNoSyncService);
    return base::nullopt;
  }

  if (!CanUploadDemographicsToGoogle(sync_service)) {
    LogUserDemographicsStatusInHistogram(
        UserDemographicsStatus::kSyncNotEnabled);
    return base::nullopt;
  }

  UserDemographicsResult demographics_result =
      GetUserNoisedBirthYearAndGenderFromPrefs(
          profile_client_->GetNetworkTime(), profile_client_->GetPrefService());
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
    UserDemographicsStatus status) {
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
