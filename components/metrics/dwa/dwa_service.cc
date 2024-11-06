// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_service.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/timezone.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "components/metrics/dwa/dwa_pref_names.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

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

// Number of seconds in a week or seven days. (604800 = 7 * 24 * 60 * 60)
const int kOneWeekInSeconds = base::Days(7).InSeconds();

const size_t kMinLogQueueCount = 10;
const size_t kMinLogQueueSizeBytes = 300 * 1024;  // 300 KiB
const size_t kMaxLogSizeBytes = 1024 * 1024;      // 1 MiB

DwaService::DwaService(MetricsServiceClient* client, PrefService* local_state)
    : recorder_(DwaRecorder::Get()),
      client_(client),
      pref_service_(local_state),
      reporting_service_(client, local_state, GetLogStoreLimits()) {
  reporting_service_.Initialize();
  // Set up the scheduler for DwaService.
  auto rotate_callback = base::BindRepeating(&DwaService::RotateLog,
                                             self_ptr_factory_.GetWeakPtr());
  auto get_upload_interval_callback =
      base::BindRepeating(&metrics::MetricsServiceClient::GetUploadInterval,
                          base::Unretained(client_));
  bool fast_startup_for_testing = client_->ShouldStartUpFastForTesting();
  scheduler_ = std::make_unique<MetricsRotationScheduler>(
      rotate_callback, get_upload_interval_callback, fast_startup_for_testing);
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
  if (!recorder_->HasPageLoadEvents()) {
    return;
  }

  BuildAndStoreLog(reason);
  reporting_service_.unsent_log_store()->TrimAndPersistUnsentLogs(true);
}

void DwaService::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recorder_->Purge();
  reporting_service_.unsent_log_store()->Purge();
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
#elif BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

  int64_t seconds_since_install =
      MetricsLog::GetCurrentTime() -
      local_state.GetInt64(metrics::prefs::kInstallDate);
  coarse_system_info->set_client_age(
      seconds_since_install < kOneWeekInSeconds
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

void DwaService::RotateLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!reporting_service_.unsent_log_store()->has_unsent_logs()) {
    BuildAndStoreLog(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  }
  reporting_service_.Start();
  scheduler_->RotationFinished();
}

void DwaService::BuildAndStoreLog(
    metrics::MetricsLogsEventManager::CreateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There are no new page load events, so no new logs should be created.
  if (!recorder_->HasPageLoadEvents()) {
    return;
  }

  ::dwa::DeidentifiedWebAnalyticsReport report;
  RecordCoarseSystemInformation(*client_, *pref_service_,
                                report.mutable_coarse_system_info());
  report.set_dwa_ephemeral_id(GetEphemeralClientId(*pref_service_));

  std::vector<::dwa::PageLoadEvents> page_load_events =
      recorder_->TakePageLoadEvents();
  report.mutable_page_load_events()->Add(
      std::make_move_iterator(page_load_events.begin()),
      std::make_move_iterator(page_load_events.end()));

  report.set_timestamp(MetricsLog::GetCurrentTime());

  std::string serialized_log;
  report.SerializeToString(&serialized_log);

  LogMetadata metadata;
  reporting_service_.unsent_log_store()->StoreLog(serialized_log, metadata,
                                                  reason);
}

// static
void DwaService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterUint64Pref(prefs::kDwaClientId, 0u);
  registry->RegisterTimePref(prefs::kDwaClientIdLastUpdated, base::Time());
  DwaReportingService::RegisterPrefs(registry);
}

metrics::UnsentLogStore* DwaService::unsent_log_store() {
  return reporting_service_.unsent_log_store();
}

}  // namespace metrics::dwa
