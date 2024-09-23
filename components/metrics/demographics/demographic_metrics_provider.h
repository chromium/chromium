// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEMOGRAPHICS_DEMOGRAPHIC_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_DEMOGRAPHICS_DEMOGRAPHIC_METRICS_PROVIDER_H_

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/ukm_demographic_metrics_provider.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

class PrefService;

namespace syncer {
class SyncService;
}

namespace metrics {

// Feature switch to report user's noised birth year and gender.
BASE_DECLARE_FEATURE(kDemographicMetricsReporting);

// Provider of the synced user’s noised birth year and gender to the UMA metrics
// server. The synced user's birth year and gender were provided to Google when
// the user created their Google account, to use in accordance with Google's
// privacy policy. The provided birth year and gender are used in aggregate and
// anonymized form to measure usage of Chrome features by age groups and gender
// - helping Chrome ensure features are useful to a wide range of users. Users
// can avoid aggregation of usage data by birth year and gender by either a)
// turning off sending usage statistics to Google or b) turning off sync.
class DemographicMetricsProvider : public MetricsProvider,
                                   public UkmDemographicMetricsProvider {
 public:
  // Interface that represents the client that retrieves Profile information.
  class ProfileClient {
   public:
    virtual ~ProfileClient() = default;

    // Gets the total number of profiles that are on disk (loaded + not loaded)
    // for the browser.
    virtual int GetNumberOfProfilesOnDisk() = 0;

    // Gets a pointer to the SyncService of the profile, which is required to
    // determine whether priority preferences carrying demographics information
    // is being synced.
    virtual syncer::SyncService* GetSyncService() = 0;

    // Gets a pointer to the PrefService for the Local State of the device.
    virtual PrefService* GetLocalState() = 0;

    // Gets a pointer to the PrefService of the user profile.
    virtual PrefService* GetProfilePrefs() = 0;

    // Gets the network time that represents now.
    // TODO(crbug.com/40729596): Remove this function and replace with
    // base::Time::Now().
    virtual base::Time GetNetworkTime() const = 0;
  };

  DemographicMetricsProvider(
      std::unique_ptr<ProfileClient> profile_client,
      MetricsLogUploader::MetricServiceType metrics_service_type);

  DemographicMetricsProvider(const DemographicMetricsProvider&) = delete;
  DemographicMetricsProvider& operator=(const DemographicMetricsProvider&) =
      delete;

  ~DemographicMetricsProvider() override;

  // Provides the synced user's noised birth year and gender to a metrics report
  // of type ReportType. This function is templated to support any type of proto
  // metrics report, e.g., ukm::Report and metrics::ChromeUserMetricsExtension.
  // The ReportType should be a proto message class that has a
  // metrics::UserDemographicsProto message field.
  template <class ReportType>
  void ProvideSyncedUserNoisedBirthYearAndGender(ReportType* report) {
    DCHECK(report);

    std::optional<UserDemographics> user_demographics =
        ProvideSyncedUserNoisedBirthYearAndGender();
    if (user_demographics.has_value()) {
      report->mutable_user_demographics()->set_birth_year(
          user_demographics.value().birth_year);
      report->mutable_user_demographics()->set_gender(
          user_demographics.value().gender);
    }
  }

  // MetricsProvider:
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;

  // UkmDemographicMetricsProvider:
  void ProvideSyncedUserNoisedBirthYearAndGenderToReport(
      ukm::Report* report) override;

 private:
  // Provides the synced user's noised birth year and gender.
  std::optional<UserDemographics> ProvideSyncedUserNoisedBirthYearAndGender();

  void LogUserDemographicsStatusInHistogram(UserDemographicsStatus status);

  std::unique_ptr<ProfileClient> profile_client_;

  // The type of the metrics service for which to emit the user demographics
  // status histogram (e.g., UMA).
  const MetricsLogUploader::MetricServiceType metrics_service_type_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEMOGRAPHICS_DEMOGRAPHIC_METRICS_PROVIDER_H_
