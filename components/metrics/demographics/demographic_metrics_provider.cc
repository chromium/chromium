// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographics/demographic_metrics_provider.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace metrics {

namespace {

bool IsValidUploadState(syncer::UploadState upload_state) {
  switch (upload_state) {
    case syncer::UploadState::NOT_ACTIVE:
      return false;
    case syncer::UploadState::INITIALIZING:
      // Note that INITIALIZING is considered good enough, because sync is known
      // to be enabled, and transient errors don't really matter here.
    case syncer::UploadState::ACTIVE:
      return true;
  }
  NOTREACHED();
}

bool CanUploadDemographicsToGoogle(syncer::SyncService* sync_service) {
  CHECK(sync_service);

  // PRIORITY_PREFERENCES is the sync datatype used to propagate demographics
  // information to the client. In its absence, demographics info is unavailable
  // thus cannot be uploaded.
  if (!IsValidUploadState(syncer::GetUploadToGoogleState(
          sync_service, syncer::PRIORITY_PREFERENCES))) {
    return false;
  }

  // Even if GetUploadToGoogleState() reports to be active, the user may be in
  // transport mode or full-sync (aka sync-the-feature enabled) mode.
  // If `kReplaceSyncPromosWithSignInPromos` is enabled, then
  // PRIORITY_PREFERENCES being enabled (which implies the user is signed in) is
  // enough, and the sync mode doesn't matter.
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return true;
  }

  // If `kReplaceSyncPromosWithSignInPromos` is NOT enabled, then demographics
  // may only be uploaded for users who have opted in to Sync.
  // TODO(crbug.com/40066949): Simplify once IsSyncFeatureEnabled() is deleted
  // from the codebase.
  if (sync_service->IsSyncFeatureEnabled()) {
    return true;
  }

  return false;
}

}  // namespace

// static
BASE_FEATURE(kDemographicMetricsReporting,
             "DemographicMetricsReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

DemographicMetricsProvider::DemographicMetricsProvider(
    std::unique_ptr<ProfileClient> profile_client,
    MetricsLogUploader::MetricServiceType metrics_service_type)
    : profile_client_(std::move(profile_client)),
      metrics_service_type_(metrics_service_type) {
  DCHECK(profile_client_);
}

DemographicMetricsProvider::~DemographicMetricsProvider() = default;

std::optional<UserDemographics>
DemographicMetricsProvider::ProvideSyncedUserNoisedBirthYearAndGender() {
  // Skip if feature disabled.
  if (!base::FeatureList::IsEnabled(kDemographicMetricsReporting))
    return std::nullopt;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Skip if not exactly one Profile on disk. Having more than one Profile that
  // is using the browser can make demographics less relevant. This approach
  // cannot determine if there is more than 1 distinct user using the Profile.

  // ChromeOS almost always has more than one profile on disk, so this check
  // doesn't work. We have a profile selection strategy for ChromeOS, so skip
  // this check for ChromeOS.
  // TODO(crbug.com/40729596): LaCros will behave similarly to desktop Chrome
  // and reduce the number of profiles on disk to one, so remove these #if
  // guards after LaCros release.
  if (profile_client_->GetNumberOfProfilesOnDisk() != 1) {
    LogUserDemographicsStatusInHistogram(
        UserDemographicsStatus::kMoreThanOneProfile);
    return std::nullopt;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  syncer::SyncService* sync_service = profile_client_->GetSyncService();
  // Skip if no sync service.
  if (!sync_service) {
    LogUserDemographicsStatusInHistogram(
        UserDemographicsStatus::kNoSyncService);
    return std::nullopt;
  }

  if (!CanUploadDemographicsToGoogle(sync_service)) {
    LogUserDemographicsStatusInHistogram(
        UserDemographicsStatus::kSyncNotEnabled);
    return std::nullopt;
  }

  UserDemographicsResult demographics_result =
      GetUserNoisedBirthYearAndGenderFromPrefs(
          profile_client_->GetNetworkTime(), profile_client_->GetLocalState(),
          profile_client_->GetProfilePrefs());
  LogUserDemographicsStatusInHistogram(demographics_result.status());

  if (demographics_result.IsSuccess())
    return demographics_result.value();

  return std::nullopt;
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
      // If the user demographics data was retrieved successfully, then the user
      // must be between the ages of |kUserDemographicsMinAgeInYears|+1=21 and
      // |kUserDemographicsMaxAgeInYears|=85, so the user is not a minor.
      base::UmaHistogramBoolean("UMA.UserDemographics.IsNoisedAgeOver21Under85",
                                status == UserDemographicsStatus::kSuccess);
      return;
    case MetricsLogUploader::MetricServiceType::UKM:
      // UKM Metrics doesn't have demographic metrics.
      return;
    case MetricsLogUploader::MetricServiceType::STRUCTURED_METRICS:
      // Structured Metrics doesn't have demographic metrics.
      return;
    case MetricsLogUploader::MetricServiceType::DWA:
      // DWA doesn't have demographic metrics.
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace metrics
