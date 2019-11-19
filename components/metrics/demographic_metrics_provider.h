// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_

#include <memory>

#include "base/time/time.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/ukm_demographic_metrics_provider.h"
#include "components/sync/base/user_demographics.h"
#include "components/sync/driver/sync_service.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

namespace base {
struct Feature;
}

namespace metrics {

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

    // Gets a weak pointer to the ProfileSyncService of the Profile.
    virtual syncer::SyncService* GetSyncService() = 0;

    // Gets the network time that represents now.
    virtual base::Time GetNetworkTime() const = 0;
  };

  DemographicMetricsProvider(
      std::unique_ptr<ProfileClient> profile_client,
      MetricsLogUploader::MetricServiceType metrics_service_type);
  ~DemographicMetricsProvider() override;

  // Provides the synced user's noised birth year and gender to a metrics report
  // of type ReportType. This function is templated to support any type of proto
  // metrics report, e.g., ukm::Report and metrics::ChromeUserMetricsExtension.
  // The ReportType should be a proto message class that has a
  // metrics::UserDemographicsProto message field.
  template <class ReportType>
  void ProvideSyncedUserNoisedBirthYearAndGender(ReportType* report) {
    DCHECK(report);

    base::Optional<syncer::UserDemographics> user_demographics =
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

  // Feature switch to report user's noised birth year and gender.
  static const base::Feature kDemographicMetricsReporting;

 protected:
  // Provides the synced user's noised birth year and gender. Virtual for
  // testing.
  virtual base::Optional<syncer::UserDemographics>
  ProvideSyncedUserNoisedBirthYearAndGender();

 private:
  void LogUserDemographicsStatusInHistogram(
      syncer::UserDemographicsStatus status);

  std::unique_ptr<ProfileClient> profile_client_;

  // The type of the metrics service for which to emit the user demographics
  // status histogram (e.g., UMA).
  const MetricsLogUploader::MetricServiceType metrics_service_type_;

  DISALLOW_COPY_AND_ASSIGN(DemographicMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEMOGRAPHIC_METRICS_PROVIDER_H_