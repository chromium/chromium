// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/puma_service.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/puma_histogram_functions.h"
#include "base/rand_util.h"
#include "base/version.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/private_metrics/private_metrics_features.h"
#include "components/metrics/private_metrics/private_metrics_pref_names.h"
#include "components/metrics/private_metrics/puma_histogram_encoder.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/version_info/version_info.h"
#include "third_party/metrics_proto/private_metrics/private_metrics.pb.h"
#include "third_party/metrics_proto/private_metrics/private_user_metrics.pb.h"
#include "third_party/metrics_proto/private_metrics/system_profiles/coarse_system_profile.pb.h"
#include "third_party/metrics_proto/private_metrics/system_profiles/rc_coarse_system_profile.pb.h"

namespace metrics::private_metrics {

namespace {

// TODO(b/463573197): Unify namespaces used for code and protos
using ::private_metrics::Platform;
using ::private_metrics::PrivateMetricEndpointPayload;
using ::private_metrics::PrivateUserMetrics;
using ::private_metrics::RcCoarseSystemProfile;
using ::private_metrics::RcCoarseSystemProfile_Channel;

// Retrieves the storage parameters to control the reporting service.
UnsentLogStore::UnsentLogStoreLimits GetLogStoreLimits() {
  return UnsentLogStore::UnsentLogStoreLimits{
      .min_log_count = kPrivateMetricsPumaMinLogQueueCount.Get(),
      .min_queue_size_bytes = kPrivateMetricsPumaMinLogQueueSizeBytes.Get(),
      .max_log_size_bytes = kPrivateMetricsPumaMaxLogSizeBytes.Get(),
  };
}

PrivateMetricEndpointPayload BuildPrivateMetricEndpointPayload(
    PrivateUserMetrics private_uma_report) {
  PrivateMetricEndpointPayload payload;
  payload.set_report_type(PrivateMetricEndpointPayload::PUMA_RC);
  payload.mutable_private_uma_report()->Swap(&private_uma_report);
  return payload;
}

RcCoarseSystemProfile_Channel MapChannelToRcChannel(
    SystemProfileProto::Channel channel) {
  switch (channel) {
    case SystemProfileProto::CHANNEL_CANARY:
      return RcCoarseSystemProfile::CHANNEL_CANARY;
    case SystemProfileProto::CHANNEL_DEV:
      return RcCoarseSystemProfile::CHANNEL_DEV;
    case SystemProfileProto::CHANNEL_BETA:
      return RcCoarseSystemProfile::CHANNEL_BETA;
    case SystemProfileProto::CHANNEL_STABLE:
      return RcCoarseSystemProfile::CHANNEL_STABLE;
    default:
      return RcCoarseSystemProfile::CHANNEL_UNKNOWN;
  }
}

Platform GetCurrentPlatform() {
#if BUILDFLAG(IS_WIN)
  return Platform::PLATFORM_WINDOWS;
#elif BUILDFLAG(IS_MAC)
  return Platform::PLATFORM_MACOS;
#elif BUILDFLAG(IS_LINUX)
  return Platform::PLATFORM_LINUX;
#elif BUILDFLAG(IS_ANDROID)
  // TODO(b/463580425): Differentiate between Android platforms.
  return Platform::PLATFORM_ANDROID;
#elif BUILDFLAG(IS_IOS)
  return Platform::PLATFORM_IOS;
#elif BUILDFLAG(IS_CHROMEOS)
  return Platform::PLATFORM_CHROMEOS;
#else
  return Platform::PLATFORM_OTHER;
#endif
}

}  // namespace

PumaService::PumaService(MetricsServiceClient* client, PrefService* local_state)
    : client_(client),
      local_state_(local_state),
      reporting_service_(client, local_state, GetLogStoreLimits(), false) {
  reporting_service_.Initialize();

  // Set up the scheduler for PumaService.
  auto rotate_callback = base::BindRepeating(&PumaService::RotateLog,
                                             self_ptr_factory_.GetWeakPtr());
  auto get_upload_interval_callback = base::BindRepeating(
      &MetricsServiceClient::GetUploadInterval, base::Unretained(client_));
  bool fast_startup = client_->ShouldStartUpFast();
  scheduler_ = std::make_unique<metrics::MetricsRotationScheduler>(
      rotate_callback, get_upload_interval_callback, fast_startup);
  scheduler_->InitTaskComplete();
}

PumaService::~PumaService() {
  DisableReporting();
}

PrivateMetricsReportingService* PumaService::reporting_service() {
  return &reporting_service_;
}

void PumaService::EnableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (reporting_service_.reporting_active()) {
    return;
  }

  scheduler_->Start();
  reporting_service_.EnableReporting();
}

void PumaService::DisableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!reporting_service_.reporting_active()) {
    return;
  }

  reporting_service_.DisableReporting();
  scheduler_->Stop();
  Flush(metrics::MetricsLogsEventManager::CreateReason::kServiceShutdown);
}

void PumaService::Flush(metrics::MetricsLogsEventManager::CreateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BuildPrivateMetricRcReportAndStoreLog(reason);
  reporting_service_.unsent_log_store()->TrimAndPersistUnsentLogs(true);
}

// static
bool PumaService::IsPumaEnabled() {
  if (base::FeatureList::IsEnabled(kPrivateMetricsFeature)) {
    // Note: PUMA should never be enabled with Private Metrics, as PUMA's
    // implementation assumes that the Private Metrics feature is disabled.
    return false;
  }

  return base::FeatureList::IsEnabled(kPrivateMetricsPuma);
}

// static
void PumaService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterUint64Pref(prefs::kPumaRcClientId, 0u);
  // PrivateMetricsReportingService prefs are registered already by DWA.
}

std::optional<uint64_t> PumaService::GetPumaRcClientId() {
  if (!base::FeatureList::IsEnabled(kPrivateMetricsPumaRc)) {
    // This is a failsafe mechanism that prevents reporting IDs in case the
    // feature is disabled.
    return std::nullopt;
  }

  uint64_t client_id = local_state_->GetUint64(prefs::kPumaRcClientId);

  // Generate the ID in case it's not available.
  if (client_id == 0u) {
    while (client_id == 0u) {
      client_id = base::RandUint64();
    }
    local_state_->SetUint64(prefs::kPumaRcClientId, client_id);
  }

  return client_id;
}

void PumaService::RotateLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!reporting_service_.unsent_log_store()->has_unsent_logs()) {
    base::UmaHistogramEnumeration(
        kHistogramPumaLogRotationOutcome,
        PumaService::LogRotationOutcome::kLogRotationPerformed);

    BuildPrivateMetricRcReportAndStoreLog(
        metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  } else {
    base::UmaHistogramEnumeration(
        kHistogramPumaLogRotationOutcome,
        PumaService::LogRotationOutcome::kLogRotationSkipped);
  }
  reporting_service_.Start();
  scheduler_->RotationFinished();
}

void PumaService::RecordRcProfile(RcCoarseSystemProfile* rc_profile) {
  rc_profile->set_channel(MapChannelToRcChannel(client_->GetChannel()));

  rc_profile->set_is_extended_stable_channel(
      client_->IsExtendedStableChannel());

  rc_profile->set_platform(GetCurrentPlatform());
  rc_profile->set_milestone(version_info::GetMajorVersionNumberAsInt());

  int country_id;
  const std::optional<regional_capabilities::CountryIdHolder>
      profile_country_id =
          client_->GetProfileCountryIdForPrivateMetricsReporting();
  if (profile_country_id.has_value()) {
    country_id = profile_country_id.value()
                     .GetRestricted(regional_capabilities::CountryAccessKey(
                         regional_capabilities::CountryAccessReason::
                             kPrivateUserMetricsReporting))
                     .Serialize();
  } else {
    country_id = country_codes::CountryId().Serialize();
  }
  rc_profile->set_profile_country_id(country_id);
}

std::optional<::private_metrics::PrivateUserMetrics>
PumaService::BuildPrivateMetricRcReport() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(kPrivateMetricsPumaRc)) {
    base::UmaHistogramEnumeration(
        kHistogramPumaReportBuildingOutcomeRc,
        PumaService::ReportBuildingOutcome::kNotBuiltFeatureDisabled);
    return std::nullopt;
  }

  ::private_metrics::PrivateUserMetrics report;
  PumaHistogramEncoder::EncodeHistogramDeltas(base::PumaType::kRc, report);

  if (report.histogram_events_size() == 0) {
    // No histograms to report.
    base::UmaHistogramEnumeration(
        kHistogramPumaReportBuildingOutcomeRc,
        PumaService::ReportBuildingOutcome::kNotBuiltNoData);
    return std::nullopt;
  }

  std::optional<uint64_t> client_id = GetPumaRcClientId();
  if (!client_id.has_value()) {
    base::UmaHistogramEnumeration(
        kHistogramPumaReportBuildingOutcomeRc,
        PumaService::ReportBuildingOutcome::kNotBuiltNoClientId);
    return std::nullopt;
  }
  report.set_client_id(client_id.value());
  RecordRcProfile(report.mutable_rc_profile());

  base::UmaHistogramEnumeration(kHistogramPumaReportBuildingOutcomeRc,
                                PumaService::ReportBuildingOutcome::kBuilt);
  return std::move(report);
}

void PumaService::BuildPrivateMetricRcReportAndStoreLog(
    metrics::MetricsLogsEventManager::CreateReason reason) {
  std::optional<::private_metrics::PrivateUserMetrics> report =
      BuildPrivateMetricRcReport();

  if (!report) {
    // No report to store.
    base::UmaHistogramEnumeration(
        kHistogramPumaReportStoringOutcomeRc,
        PumaService::ReportStoringOutcome::kNotStoredNoReport);
    return;
  }

  auto payload = BuildPrivateMetricEndpointPayload(*std::move(report));

  std::string serialized_log;
  if (!payload.SerializeToString(&serialized_log)) {
    // Drop the report in case serialization fails.
    base::UmaHistogramEnumeration(
        kHistogramPumaReportStoringOutcomeRc,
        PumaService::ReportStoringOutcome::kNotStoredSerializationFailed);
    return;
  }

  LogMetadata metadata;
  reporting_service_.unsent_log_store()->StoreLog(serialized_log, metadata,
                                                  reason);

  base::UmaHistogramEnumeration(kHistogramPumaReportStoringOutcomeRc,
                                PumaService::ReportStoringOutcome::kStored);
}

regional_capabilities::CountryIdHolder
PumaService::GetCountryIdHolderForTesting() {
  return client_->GetProfileCountryIdForPrivateMetricsReporting().value();
}

}  // namespace metrics::private_metrics
