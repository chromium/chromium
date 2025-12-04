// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PUMA_SERVICE_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PUMA_SERVICE_H_

#include "components/metrics/metrics_rotation_scheduler.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/private_metrics/private_metrics_reporting_service.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/private_metrics/private_user_metrics.pb.h"

namespace private_metrics {
class RcCoarseSystemProfile;
}

namespace metrics::private_metrics {

inline constexpr const char* kHistogramPumaLogRotationOutcome =
    "PrivateMetrics.PUMA.LogRotationOutcome";

inline constexpr const char* kHistogramPumaReportBuildingOutcomeRc =
    "PrivateMetrics.PUMA.ReportBuildingOutcome.Rc";
inline constexpr const char* kHistogramPumaReportStoringOutcomeRc =
    "PrivateMetrics.PUMA.ReportStoringOutcome.Rc";

// PumaService is responsible for uploading Private UMA histograms.
class PumaService {
 public:
  // LINT.IfChange(PumaReportBuildingOutcome)
  enum class ReportBuildingOutcome {
    kBuilt = 0,
    kNotBuiltFeatureDisabled = 1,
    kNotBuiltNoData = 2,
    kNotBuiltNoClientId = 3,
    kMaxValue = kNotBuiltNoClientId,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/private_metrics/enums.xml:PumaReportBuildingOutcome)

  // LINT.IfChange(PumaReportStoringOutcome)
  enum class ReportStoringOutcome {
    kStored = 0,
    kNotStoredNoReport = 1,
    kNotStoredSerializationFailed = 1,
    kMaxValue = kNotStoredSerializationFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/private_metrics/enums.xml:PumaReportStoringOutcome)

  // LINT.IfChange(PumaLogRotationOutcome)
  enum class LogRotationOutcome {
    kLogRotationPerformed = 0,
    kLogRotationSkipped = 1,
    kMaxValue = kLogRotationSkipped,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/private_metrics/enums.xml:PumaLogRotationOutcome)

  PumaService(MetricsServiceClient* client, PrefService* pref_service);
  PumaService(const PumaService&) = delete;
  PumaService& operator=(const PumaService&) = delete;

  ~PumaService();

  PrivateMetricsReportingService* reporting_service();

  void EnableReporting();
  void DisableReporting();

  // Flushes any event currently in the recorder to prefs.
  void Flush(metrics::MetricsLogsEventManager::CreateReason reason);

  // Returns true if the PUMA feature is enabled, false otherwise.
  static bool IsPumaEnabled();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  regional_capabilities::CountryIdHolder GetCountryIdHolderForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(PumaServiceRcTest,
                           RcBuildReportAndStore_DoesCreateAndStoreReport);
  FRIEND_TEST_ALL_PREFIXES(PumaServiceRcTest,
                           RcBuildReportAndStore_DoesNotStoreReportWithNoData);
  FRIEND_TEST_ALL_PREFIXES(PumaServiceRcTest,
                           RcBuildReport_DoesCreateReportWithEvents);
  FRIEND_TEST_ALL_PREFIXES(PumaServiceRcTest,
                           RcBuildReport_DoesNotCreateReportWithoutEvents);
  FRIEND_TEST_ALL_PREFIXES(PumaServiceRcTest,
                           RcBuildReport_PayloadProperlyFilled);
  FRIEND_TEST_ALL_PREFIXES(PumaServiceRcTest, RcClientId_IsNonNull);
  FRIEND_TEST_ALL_PREFIXES(PumaServiceRcTest,
                           RcClientId_SameWhenExecutedMultipleTimes);
  FRIEND_TEST_ALL_PREFIXES(PumaServiceRcTest, RcClientId_UpdatesPref);
  FRIEND_TEST_ALL_PREFIXES(PumaServiceRcTest, RcRecordCoarseSystemProfile);
  FRIEND_TEST_ALL_PREFIXES(
      PumaServiceTest,
      RcBuildReport_DoesNotCreateReportWithFeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(PumaServiceTest,
                           RcClientId_IsNullWhenPumaRcIsDisabled);

  // Gets or generates client ID for PUMA for Regional Capabilities.
  //
  // This client ID is persistent and not joinable with any other client ID.
  // `std::nullopt` is returned when the feature is disabled, and IDs should not
  // be collected.
  std::optional<uint64_t> GetPumaRcClientId();

  // Records coarse system profile for PUMA for Regional Capabilities.
  void RecordRcProfile(::private_metrics::RcCoarseSystemProfile* rc_profile);

  // Constructs a new PrivateMetricReport from available data from PUMA for
  // Regional Capabilities, and returns it.
  //
  // Marks the data as processed, and makes sure it will not be processed again.
  std::optional<::private_metrics::PrivateUserMetrics>
  BuildPrivateMetricRcReport();

  // Constructs a new PrivateMetricReport from available data from PUMA for
  // Regional Capabilities, and stores it in the unsent log store.
  //
  // Marks the data as processed, and makes sure it will not be processed again.
  void BuildPrivateMetricRcReportAndStoreLog(
      metrics::MetricsLogsEventManager::CreateReason reason);

  // Periodically called by `scheduler_` to advance processing of logs.
  void RotateLog();

  SEQUENCE_CHECKER(sequence_checker_);

  // The metrics client `this` is service is associated with.
  raw_ptr<MetricsServiceClient> client_;

  // Local state; preferences not associated with a specific profile.
  raw_ptr<PrefService> local_state_;

  // Service for uploading serialized logs to Private Metrics endpoint.
  PrivateMetricsReportingService reporting_service_;

  // The scheduler for determining when uploads should happen.
  std::unique_ptr<MetricsRotationScheduler> scheduler_;

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as PumaService.
  base::WeakPtrFactory<PumaService> self_ptr_factory_{this};
};

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PUMA_SERVICE_H_
