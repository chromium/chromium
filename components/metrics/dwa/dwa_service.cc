// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/timezone.h"
#include "base/json/json_writer.h"
#include "base/metrics/metrics_hashes.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "components/metrics/dwa/dwa_pref_names.h"
#include "components/metrics/dwa/dwa_rotation_scheduler.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/private_metrics/private_metrics_features.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/crypto.h"

namespace metrics::dwa {

// Set of countries in the European Economic Area. Used by
// RecordCoarseSystemInformation to set geo_designation fields in
// CoarseSystemInfo. These will need to be manually updated using
// "IsEuropeanEconomicArea" from go/source/user_preference_country.impl.gcl.
constexpr auto kEuropeanEconomicAreaCountries =
    base::MakeFixedFlatSet<std::string_view>({
        "at",  // Austria
        "be",  // Belgium
        "bg",  // Bulgaria
        "hr",  // Croatia
        "cy",  // Cyprus
        "cz",  // Czech Republic
        "dk",  // Denmark
        "ee",  // Estonia
        "fi",  // Finland
        "fr",  // France
        "de",  // Germany
        "gr",  // Greece
        "hu",  // Hungary
        "is",  // Iceland
        "ie",  // Ireland
        "it",  // Italy
        "lv",  // Latvia
        "li",  // Liechtenstein
        "lt",  // Lithuania
        "lu",  // Luxembourg
        "mt",  // Malta
        "nl",  // Netherlands
        "no",  // Norway
        "pl",  // Poland
        "pt",  // Portugal
        "ro",  // Romania
        "sk",  // Slovakia
        "si",  // Slovenia
        "es",  // Spain
        "se",  // Sweden
        "uk",  // United Kingdom
    });

// One week or seven days represented in base::TimeDelta.
const base::TimeDelta kOneWeek = base::Days(7);

const size_t kMinLogQueueCount = 10;
const size_t kMinLogQueueSizeBytes = 300 * 1024;  // 300 KiB
const size_t kMaxLogSizeBytes = 1024 * 1024;      // 1 MiB

DwaService::DwaService(
    MetricsServiceClient* client,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : recorder_(DwaRecorder::Get()),
      client_(client),
      pref_service_(local_state),
      reporting_service_(client, local_state, GetLogStoreLimits()) {
  reporting_service_.Initialize();

  // Set up the downloader to refresh the encryption public key.
  data_upload_config_downloader_ =
      std::make_unique<private_metrics::DataUploadConfigDownloader>(
          url_loader_factory);

  encryption_public_key_verifier_ =
      base::BindRepeating(&ValidateEncryptionPublicKey);

  // Set up the scheduler for DwaService.
  auto rotate_callback = base::BindRepeating(&DwaService::RotateLog,
                                             self_ptr_factory_.GetWeakPtr());
  auto get_upload_interval_callback =
      base::BindRepeating(&metrics::MetricsServiceClient::GetUploadInterval,
                          base::Unretained(client_));
  bool fast_startup = client_->ShouldStartUpFast();
  scheduler_ = std::make_unique<DwaRotationScheduler>(
      rotate_callback, get_upload_interval_callback, fast_startup);
  scheduler_->InitTaskComplete();
}

DwaService::~DwaService() {
  recorder_->DisableRecording();
  DisableReporting();
}

void DwaService::EnableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (reporting_service_.reporting_active()) {
    return;
  }

  scheduler_->Start();
  reporting_service_.EnableReporting();
  // Attempt to upload if there are unsent logs.
  if (reporting_service_.unsent_log_store()->has_unsent_logs()) {
    reporting_service_.Start();
  }
}

void DwaService::DisableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reporting_service_.DisableReporting();
  scheduler_->Stop();
  Flush(metrics::MetricsLogsEventManager::CreateReason::kServiceShutdown);
}

void DwaService::Flush(metrics::MetricsLogsEventManager::CreateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The log should not be built if there aren't any events to log.
  if (!recorder_->HasEntries()) {
    return;
  }

  if (base::FeatureList::IsEnabled(private_metrics::kPrivateMetricsFeature)) {
    BuildPrivateMetricReportAndStoreLog(reason);
  } else {
    BuildDwaReportAndStoreLog(reason);
  }
  reporting_service_.unsent_log_store()->TrimAndPersistUnsentLogs(true);
}

void DwaService::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recorder_->Purge();
  reporting_service_.unsent_log_store()->Purge();
}

void DwaService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DwaService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<fcp::confidential_compute::OkpCwt>
DwaService::GetEncryptionPublicKey() {
  auto cwt = fcp::confidential_compute::OkpCwt::Decode(encryption_public_key_);
  if (!cwt.ok() || !IsValidCwt(*cwt)) {
    return std::nullopt;
  }
  return *cwt;
}

void DwaService::RefreshEncryptionPublicKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Multiple calls to FetchDataUploadConfig() is acceptable because
  // FetchDataUploadConfig() checks for an existing fetch and discards any
  // additional attempts if one is already in progress.
  data_upload_config_downloader_->FetchDataUploadConfig(
      base::BindOnce(&DwaService::HandleEncryptionPublicKeyRefresh,
                     self_ptr_factory_.GetWeakPtr()));
}

void DwaService::SetEncryptionPublicKeyForTesting(
    std::string_view test_encryption_public_key) {
  encryption_public_key_ = test_encryption_public_key;
}

void DwaService::SetEncryptionPublicKeyVerifierForTesting(
    const base::RepeatingCallback<
        bool(const fcp::confidential_compute::OkpCwt&)>&
        test_encryption_public_key_verifier) {
  encryption_public_key_verifier_ = test_encryption_public_key_verifier;
}

void DwaService::HandleEncryptionPublicKeyRefresh(
    std::optional<fcp::confidentialcompute::DataUploadConfig>
        maybe_data_upload_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `maybe_data_upload_config` may be empty if the http
  // request to fetch `fcp::confidentialcompute::DataUploadConfig` results in an
  // empty response body or if the HTTP status is non-200.
  // `maybe_data_upload_config` may also be empty if de-serialization of
  // DataUploadConfig into a protocol buffer object fails.
  // If HandleEncryptionPublicKeyRefresh() fails to update or refresh the public
  // key in one of the described error scenarios, the method will maintain the
  // previous public key value.
  if (maybe_data_upload_config.has_value() &&
      // The field may be empty in the unexpected case that the protocol buffer
      // is malformed.
      maybe_data_upload_config.value().has_confidential_encryption_config()) {
    auto encryption_public_key = maybe_data_upload_config.value()
                                     .confidential_encryption_config()
                                     .public_key();

    // Validate the public key is a valid cwt before setting the in-memory
    // field, `encryption_public_key_`. This function call is okay to use in
    // main thread. The performance of decoding of CBOR web tokens (CWT) is
    // generally considered to be more efficient than the decoding of JSON Web
    // Token (JWT) decoding due to CBOR's binary nature.
    auto cwt = fcp::confidential_compute::OkpCwt::Decode(encryption_public_key);
    if (!cwt.ok() || !IsValidCwt(*cwt)) {
      return;
    }

    encryption_public_key_ = encryption_public_key;
    for (Observer& observer : observers_) {
      observer.OnEncryptionPublicKeyChanged(*cwt);
    }
  }
}

bool DwaService::IsValidCwt(const fcp::confidential_compute::OkpCwt& cwt) {
  return cwt.algorithm.has_value() && cwt.public_key.has_value() &&
         encryption_public_key_verifier_.Run(cwt);
}

// static
UnsentLogStore::UnsentLogStoreLimits DwaService::GetLogStoreLimits() {
  return UnsentLogStore::UnsentLogStoreLimits{
      .min_log_count = kMinLogQueueCount,
      .min_queue_size_bytes = kMinLogQueueSizeBytes,
      .max_log_size_bytes = kMaxLogSizeBytes,
  };
}

// static
void DwaService::RecordCoarseSystemInformation(
    MetricsServiceClient& client,
    const PrefService& local_state,
    ::dwa::CoarseSystemInfo* coarse_system_info) {
  switch (client.GetChannel()) {
    case SystemProfileProto::CHANNEL_STABLE:
      coarse_system_info->set_channel(::dwa::CoarseSystemInfo::CHANNEL_STABLE);
      break;
    case SystemProfileProto::CHANNEL_CANARY:
    case SystemProfileProto::CHANNEL_DEV:
    case SystemProfileProto::CHANNEL_BETA:
      coarse_system_info->set_channel(
          ::dwa::CoarseSystemInfo::CHANNEL_NOT_STABLE);
      break;
    case SystemProfileProto::CHANNEL_UNKNOWN:
      coarse_system_info->set_channel(::dwa::CoarseSystemInfo::CHANNEL_INVALID);
      break;
  }

#if BUILDFLAG(IS_WIN)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_WINDOWS);
#elif BUILDFLAG(IS_MAC)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_MACOS);
#elif BUILDFLAG(IS_LINUX)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_LINUX);
#elif BUILDFLAG(IS_ANDROID)
  // TODO(b/366276323): Populate set_platform using more granular
  // PLATFORM_ANDROID enum.
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_ANDROID);
#elif BUILDFLAG(IS_IOS)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_IOS);
#elif BUILDFLAG(IS_CHROMEOS)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_CHROMEOS);
#else
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_OTHER);
#endif

  std::string country =
      base::ToLowerASCII(base::CountryCodeForCurrentTimezone());
  if (country == "") {
    coarse_system_info->set_geo_designation(
        ::dwa::CoarseSystemInfo::GEO_DESIGNATION_INVALID);
  } else if (kEuropeanEconomicAreaCountries.contains(country)) {
    coarse_system_info->set_geo_designation(
        ::dwa::CoarseSystemInfo::GEO_DESIGNATION_EEA);
  } else {
    // GEO_DESIGNATION_ROW is the geo designation for "rest of the world".
    coarse_system_info->set_geo_designation(
        ::dwa::CoarseSystemInfo::GEO_DESIGNATION_ROW);
  }

  base::TimeDelta time_since_install =
      base::Time::Now() -
      base::Time::FromTimeT(local_state.GetInt64(metrics::prefs::kInstallDate));
  coarse_system_info->set_client_age(
      time_since_install < kOneWeek
          ? ::dwa::CoarseSystemInfo::CLIENT_AGE_RECENT
          : ::dwa::CoarseSystemInfo::CLIENT_AGE_NOT_RECENT);

  // GetVersion() returns base::Version, which represents a dotted version
  // number, like "1.2.3.4". We %16 in milestone_prefix_trimmed because it is
  // required by the DWA proto in
  // //third_party/metrics_proto/dwa/deidentified_web_analytics.proto.
  int milestone = version_info::GetVersion().components()[0];
  coarse_system_info->set_milestone_prefix_trimmed(milestone % 16);

  coarse_system_info->set_is_ukm_enabled(client.IsUkmAllowedForAllProfiles());
}

// static
std::optional<::private_metrics::PrivateMetricEndpointPayload>
DwaService::BuildPrivateMetricEndpointPayloadFromEncryptedReport(
    ::private_metrics::EncryptedPrivateMetricReport encrypted_report) {
  ::private_metrics::PrivateMetricEndpointPayload::ReportType report_type;
  switch (encrypted_report.report_type()) {
    case ::private_metrics::EncryptedPrivateMetricReport::DWA:
      report_type = ::private_metrics::PrivateMetricEndpointPayload::DWA;
      break;
    case ::private_metrics::EncryptedPrivateMetricReport::DKM:
      report_type = ::private_metrics::PrivateMetricEndpointPayload::DKM;
      break;
    case ::private_metrics::EncryptedPrivateMetricReport::REPORT_TYPE_INVALID:
      return std::nullopt;
  }

  ::private_metrics::PrivateMetricEndpointPayload payload;
  payload.set_report_type(report_type);
  payload.mutable_encrypted_private_metric_report()->Swap(
      &encrypted_report);
  return payload;
}

// static
uint64_t DwaService::GetEphemeralClientId(PrefService& local_state) {
  // We want to update the client id once a day (measured in UTC), so our date
  // should only contain information up to day level.
  base::Time now_day_level = base::Time::Now().UTCMidnight();

  uint64_t client_id = local_state.GetUint64(prefs::kDwaClientId);
  if (local_state.GetTime(prefs::kDwaClientIdLastUpdated) != now_day_level ||
      client_id == 0u) {
    client_id = 0u;
    while (!client_id) {
      client_id = base::RandUint64();
    }
    local_state.SetUint64(prefs::kDwaClientId, client_id);

    local_state.SetTime(prefs::kDwaClientIdLastUpdated, now_day_level);
  }

  return client_id;
}

// static
uint64_t DwaService::HashCoarseSystemInfo(
    const ::dwa::CoarseSystemInfo& coarse_system_info) {
  return base::HashMetricName(base::JoinString(
      {base::NumberToString(coarse_system_info.channel()),
       base::NumberToString(coarse_system_info.platform()),
       base::NumberToString(coarse_system_info.geo_designation()),
       base::NumberToString(coarse_system_info.client_age()),
       base::NumberToString(coarse_system_info.milestone_prefix_trimmed()),
       base::NumberToString(coarse_system_info.is_ukm_enabled())},
      "-"));
}

// static
std::optional<uint64_t> DwaService::HashRepeatedFieldTrials(
    const google::protobuf::RepeatedPtrField<
        ::metrics::SystemProfileProto::FieldTrial>& repeated_field_trials) {
  std::vector<std::pair<uint32_t, uint32_t>> field_trials_vector;
  field_trials_vector.reserve(repeated_field_trials.size());
  for (const auto& field_trials : repeated_field_trials) {
    field_trials_vector.emplace_back(field_trials.name_id(),
                                     field_trials.group_id());
  }

  std::sort(field_trials_vector.begin(), field_trials_vector.end());

  base::Value::List value_list;
  for (const auto& field_trials : field_trials_vector) {
    base::Value::List field_trial_pair;
    field_trial_pair.Append(base::NumberToString(field_trials.first));
    field_trial_pair.Append(base::NumberToString(field_trials.second));
    value_list.Append(std::move(field_trial_pair));
  }

  auto serialized_json = base::WriteJson(value_list);
  if (!serialized_json.has_value()) {
    return std::nullopt;
  }
  return base::HashMetricName(serialized_json.value());
}

// static
std::vector<uint64_t> DwaService::BuildKAnonymityBuckets(
    const ::dwa::DeidentifiedWebAnalyticsEvent& dwa_event) {
  auto coarse_system_info_hash =
      HashCoarseSystemInfo(dwa_event.coarse_system_info());
  auto field_trials_hash = HashRepeatedFieldTrials(dwa_event.field_trials());

  if (!field_trials_hash.has_value()) {
    return std::vector<uint64_t>();
  }

  std::vector<uint64_t> k_anonymity_buckets;
  k_anonymity_buckets.push_back(base::HashMetricName(
      base::JoinString({base::NumberToString(coarse_system_info_hash),
                        base::NumberToString(dwa_event.event_hash()),
                        base::NumberToString(field_trials_hash.value())},
                       "-")));
  k_anonymity_buckets.push_back(dwa_event.content_hash());
  return k_anonymity_buckets;
}

// static
std::optional<::private_metrics::EncryptedPrivateMetricReport>
DwaService::EncryptPrivateMetricReport(
    const ::private_metrics::PrivateMetricReport& report,
    std::string_view public_key,
    const fcp::confidential_compute::OkpCwt& decoded_public_key) {
  std::string serialized_log;
  report.SerializeToString(&serialized_log);

  ::private_metrics::PrivateMetricReportHeader report_header;
  report_header.set_key_id(decoded_public_key.public_key.value().key_id);
  report_header.set_epoch_id(report.epoch_id());

  std::string serialized_report_header;
  report_header.SerializeToString(&serialized_report_header);

  // The messages are encrypted with AEAD using a per-message generated
  // symmetric key and then the symmetric key is encrypted using HPKE with the
  // public key. This function call is considered fast for use in main thread.
  // TODO(crbug.com/444678679): Add UMA histogram to measure performance on
  // Encrypt() for detecting any potential regressions or edge cases.
  fcp::confidential_compute::MessageEncryptor message_encryptor;
  auto result = message_encryptor.Encrypt(serialized_log, public_key,
                                          serialized_report_header);
  if (!result.ok()) {
    return std::nullopt;
  }

  ::private_metrics::EncryptedPrivateMetricReport encrypted_report;
  *encrypted_report.mutable_encrypted_report() =
      std::move(result.value().ciphertext);
  encrypted_report.set_serialized_report_header(serialized_report_header);
  *encrypted_report.mutable_report_header() = std::move(report_header);
  encrypted_report.set_report_type(
      ::private_metrics::EncryptedPrivateMetricReport::DWA);
  return encrypted_report;
}

// static
bool DwaService::ValidateEncryptionPublicKey(
    const fcp::confidential_compute::OkpCwt& cwt) {
  // In the unexpected case where the `cwt` is malformed and does not contain an
  // expiration time, the key should not be used.
  if (!cwt.expiration_time.has_value()) {
    return false;
  }

  // If the encryption key is close to expiration (12 hour buffer), the key
  // should not be used.
  auto public_key_expiration = cwt.expiration_time.value() - absl::Hours(12);
  return public_key_expiration >= absl::Now();
}

void DwaService::RotateLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!reporting_service_.unsent_log_store()->has_unsent_logs()) {
    if (base::FeatureList::IsEnabled(private_metrics::kPrivateMetricsFeature)) {
      BuildPrivateMetricReportAndStoreLog(
          metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
    } else {
      BuildDwaReportAndStoreLog(
          metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
    }
  }
  reporting_service_.Start();
  scheduler_->RotationFinished();
}

void DwaService::BuildDwaReportAndStoreLog(
    metrics::MetricsLogsEventManager::CreateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There are no new events, so no new logs should be created.
  if (!recorder_->HasEntries()) {
    return;
  }

  ::dwa::DeidentifiedWebAnalyticsReport report;
  RecordCoarseSystemInformation(*client_, *pref_service_,
                                report.mutable_coarse_system_info());
  report.set_dwa_ephemeral_id(GetEphemeralClientId(*pref_service_));

  std::vector<::dwa::DeidentifiedWebAnalyticsEvent> dwa_events =
      recorder_->TakeDwaEvents();
  report.mutable_dwa_events()->Add(std::make_move_iterator(dwa_events.begin()),
                                   std::make_move_iterator(dwa_events.end()));

  report.set_timestamp(MetricsLog::GetCurrentTime());

  std::string serialized_log;
  report.SerializeToString(&serialized_log);

  LogMetadata metadata;
  reporting_service_.unsent_log_store()->StoreLog(serialized_log, metadata,
                                                  reason);
}

void DwaService::BuildPrivateMetricReportAndStoreLog(
    metrics::MetricsLogsEventManager::CreateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There are no new events, so no new logs should be created.
  if (!recorder_->HasEntries()) {
    return;
  }

  // If the encryption public key is empty, no new logs should be created.
  if (encryption_public_key_.empty()) {
    RefreshEncryptionPublicKey();
    return;
  }

  auto cwt = fcp::confidential_compute::OkpCwt::Decode(encryption_public_key_);
  if (!cwt.ok() || !IsValidCwt(*cwt)) {
    RefreshEncryptionPublicKey();
    return;
  }

  // Build the private metric report.
  ::private_metrics::PrivateMetricReport report;
  report.set_ephemeral_id(GetEphemeralClientId(*pref_service_));

  uint64_t epoch_id = (base::Time::Now() - base::Time::UnixEpoch()).InDays();
  report.set_epoch_id(epoch_id);

  std::vector<::dwa::DeidentifiedWebAnalyticsEvent> dwa_events =
      recorder_->TakeDwaEvents();

  for (auto& dwa_event : dwa_events) {
    RecordCoarseSystemInformation(*client_, *pref_service_,
                                  dwa_event.mutable_coarse_system_info());

    auto k_anonymity_buckets = BuildKAnonymityBuckets(dwa_event);
    // Since there are no k-anonymity buckets, the k-anonymity filter cannot be
    // enforced. As such, the event should be dropped.
    // TODO(crbug.com/432764678): Add UMA metric when dwa_event is dropped due
    // to empty k-anonymity buckets.
    if (k_anonymity_buckets.empty()) {
      continue;
    }

    auto* event = report.add_events();
    event->mutable_k_anonymity_buckets()->Add(
        std::make_move_iterator(k_anonymity_buckets.begin()),
        std::make_move_iterator(k_anonymity_buckets.end()));
    *event->mutable_dwa_event() = std::move(dwa_event);
  }

  auto encrypted_report =
      EncryptPrivateMetricReport(report, encryption_public_key_, cwt.value());
  if (!encrypted_report.has_value()) {
    // The encrypted report should be dropped in the unexpected event that
    // private metrics report encryption fails.
    // TODO(crbug.com/444681539): Add UMA histogram to check when private
    // metrics encryption fails.
    return;
  }
  auto encrypted_report_payload =
      BuildPrivateMetricEndpointPayloadFromEncryptedReport(
          std::move(encrypted_report.value()));
  if (!encrypted_report_payload.has_value()) {
    // The encrypted report should be dropped in the unexpected event that
    // the private metric endpoint payload could not be built.
    // TODO(crbug.com/444681539): Add UMA histogram to check when private
    // metrics endpoint payload build fails.
    return;
  }

  std::string serialized_log;
  if (!encrypted_report_payload.value().SerializeToString(&serialized_log)) {
    // The encrypted report should be dropped in the unexpected event that
    // private metrics report serialization fails.
    // TODO(crbug.com/444681539): Add UMA histogram to check when private
    // metrics serialization fails.
    return;
  }

  LogMetadata metadata;
  reporting_service_.unsent_log_store()->StoreLog(serialized_log, metadata,
                                                  reason);
}

// static
void DwaService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterUint64Pref(prefs::kDwaClientId, 0u);
  registry->RegisterTimePref(prefs::kDwaClientIdLastUpdated, base::Time());
  private_metrics::PrivateMetricsReportingService::RegisterPrefs(registry);
}

metrics::UnsentLogStore* DwaService::unsent_log_store() {
  return reporting_service_.unsent_log_store();
}

}  // namespace metrics::dwa
