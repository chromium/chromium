// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_tracker.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/net/url_translator.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/device_statistics_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace syncer {

namespace {

constexpr char kLastRecordedPref[] = "sync.device_statistics_timestamp";

// A device is considered active if it has been used within this amount of time.
constexpr base::TimeDelta kDeviceActivityTimeRange = base::Days(28);

std::string GenerateCacheGUID() {
  // Generate a GUID with 128 bits of randomness.
  constexpr int kGuidBytes = 128 / 8;
  return base::Base64Encode(base::RandBytesAsVector(kGuidBytes));
}

bool ShouldRecordOutcomeMetrics(
    DeviceStatisticsTracker::RequestsCompletedSuccess success) {
  using RequestsCompletedSuccess =
      DeviceStatisticsTracker::RequestsCompletedSuccess;
  switch (success) {
    case RequestsCompletedSuccess::kAllSucceeded:
    case RequestsCompletedSuccess::kPrimarySucceededButNonPrimaryFailed:
      return true;
    case RequestsCompletedSuccess::kPrimaryFailedButNonPrimarySucceeded:
    case RequestsCompletedSuccess::kAllFailed:
    case RequestsCompletedSuccess::kPrimaryAccountChangedOrRemoved:
      return false;
  }
  NOTREACHED();
}

std::optional<DeviceStatisticsTracker::Platform> PlatformFromProto(
    sync_pb::SyncEnums::OsType os_type) {
  switch (os_type) {
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_WINDOWS:
      return DeviceStatisticsTracker::Platform::kWindows;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_MAC:
      return DeviceStatisticsTracker::Platform::kMac;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_LINUX:
      return DeviceStatisticsTracker::Platform::kLinux;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_CHROME_OS_ASH:
      return DeviceStatisticsTracker::Platform::kChromeOS;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_ANDROID:
      return DeviceStatisticsTracker::Platform::kAndroid;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_IOS:
      return DeviceStatisticsTracker::Platform::kIOS;
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_CHROME_OS_LACROS:
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_FUCHSIA:
    case sync_pb::SyncEnums::OsType::SyncEnums_OsType_OS_TYPE_UNSPECIFIED:
      // Unknown, deprecated, or not interesting.
      return std::nullopt;
  }
  NOTREACHED();
}

absl::flat_hash_map<
    std::pair<sync_pb::SyncEnums::DeviceFormFactor, sync_pb::SyncEnums::OsType>,
    std::multimap<base::Time, int>>
GetRelevantEventsByType(
    const std::vector<sync_pb::SyncEntity>& entities,
    const std::vector<std::string>& current_device_cache_guids) {
  absl::flat_hash_map<std::pair<sync_pb::SyncEnums::DeviceFormFactor,
                                sync_pb::SyncEnums::OsType>,
                      std::multimap<base::Time, int>>
      events_by_type;

  const base::Time now = base::Time::Now();

  for (const sync_pb::SyncEntity& entity : entities) {
    const sync_pb::DeviceInfoSpecifics& device =
        entity.specifics().device_info();
    // Only consider Chrome devices (not Google Play Services).
    if (!device.has_chrome_version_info()) {
      continue;
    }

    // Don't consider the current device.
    if (std::ranges::contains(current_device_cache_guids,
                              device.cache_guid())) {
      continue;
    }

    // Only consider recently-used devices.
    base::Time last_updated_time =
        ProtoTimeToTime(device.last_updated_timestamp());
    if (now - last_updated_time > kDeviceActivityTimeRange) {
      continue;
    }

    // Perform activity-time-range based deduping, similar to
    // DeviceInfoSyncBridge::CountActiveDevicesByType(): Devices with the same
    // form factor and OS, but with non-overlapping usage times, are likely
    // the same device, just with different cache GUIDs.
    base::Time begin = syncer::ProtoTimeToTime(entity.ctime());
    base::Time end = syncer::ProtoTimeToTime(entity.mtime());
    // Begin/end timestamps are received from other devices without local
    // sanitizing, so potentially the timestamps could be malformed, and the
    // modification time may predate the creation time.
    if (begin > end) {
      continue;
    }
    sync_pb::SyncEnums::DeviceFormFactor form_factor =
        device.device_form_factor();
    sync_pb::SyncEnums::OsType os_type = device.os_type();
    events_by_type[{form_factor, os_type}].emplace(begin, 1);
    events_by_type[{form_factor, os_type}].emplace(end, -1);
  }

  return events_by_type;
}

// `events` represents a set of devices, more precisely their first and last use
// dates. First-use dates are represented as `1`, last-use dates as `-1`. This
// function computes the maximum number of devices that were used at any one
// time, i.e. the max number of devices with overlapping usage times.
int CalculateMaxConcurrentEvents(const std::multimap<base::Time, int>& events) {
  int max_overlapping = 0;
  int overlapping = 0;
  for (const auto& [time, value] : events) {
    overlapping += value;
    CHECK_LE(0, overlapping);
    max_overlapping = std::max(max_overlapping, overlapping);
  }
  CHECK_EQ(overlapping, 0);
  return max_overlapping;
}

std::vector<DeviceStatisticsTracker::Platform> GetPlatformsPerDevice(
    const absl::flat_hash_map<std::pair<sync_pb::SyncEnums::DeviceFormFactor,
                                        sync_pb::SyncEnums::OsType>,
                              std::multimap<base::Time, int>>& events_by_type) {
  std::vector<DeviceStatisticsTracker::Platform> result;
  for (const auto& [form_factor_and_os_type, events] : events_by_type) {
    // Figure out the platform/OS of the other device, and skip
    // unknown/uninteresting ones.
    std::optional<DeviceStatisticsTracker::Platform> platform =
        PlatformFromProto(form_factor_and_os_type.second);
    if (!platform) {
      continue;
    }
    int count = CalculateMaxConcurrentEvents(events);
    for (int i = 0; i < count; ++i) {
      result.push_back(*platform);
    }
  }
  return result;
}

}  // namespace

DeviceStatisticsTracker::DeviceStatisticsTracker(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    const GURL& sync_server_url,
    RequestFactory request_factory,
    std::vector<std::string> current_device_cache_guids)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_server_url_(sync_server_url),
      request_factory_(std::move(request_factory)),
      current_device_cache_guids_(std::move(current_device_cache_guids)),
      primary_account_(identity_manager_->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin)) {
  CHECK(pref_service_);
  CHECK(identity_manager_);
  CHECK(request_factory_);
}

void DeviceStatisticsTracker::Start(base::OnceClosure callback) {
  CHECK(!callback_);
  CHECK(callback);

  callback_ = std::move(callback);

  const std::vector<CoreAccountInfo> accounts =
      identity_manager_->GetAccountsWithRefreshTokens();

  // Only send requests / record metrics once per day, and only if there are
  // accounts.
  const base::Time last_recorded_at = pref_service_->GetTime(kLastRecordedPref);
  const base::Time now = base::Time::Now();
  if (primary_account_.IsEmpty() || accounts.empty() ||
      (!last_recorded_at.is_null() &&
       last_recorded_at.LocalMidnight() >= now.LocalMidnight())) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback_));
    return;
  }

  pref_service_->SetTime(kLastRecordedPref, now);

  GURL::Replacements path_replacement;
  std::string path = sync_server_url_.GetPath() + "/command/";
  path_replacement.SetPathStr(path);
  GURL base_url = sync_server_url_.ReplaceComponents(path_replacement);

  CHECK(identity_manager_->AreRefreshTokensLoaded());

  for (const CoreAccountInfo& account : accounts) {
    GURL request_url =
        syncer::AppendSyncQueryString(base_url, GenerateCacheGUID());
    requests_[account.gaia] = request_factory_.Run(account, request_url);
  }

  for (const auto& [gaia, request] : requests_) {
    // Note: Unretained() is safe because `this` owns the request, and the
    // request will not run its callback after being deleted itself.
    request->Start(
        base::BindOnce(&DeviceStatisticsTracker::RequestDoneForGaiaId,
                       base::Unretained(this), gaia));
  }

  base::UmaHistogramCounts100("Sync.DeviceStatistics.RequestsStartedCount",
                              requests_.size());
}

DeviceStatisticsTracker::~DeviceStatisticsTracker() = default;

// static
void DeviceStatisticsTracker::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastRecordedPref, base::Time());
}

void DeviceStatisticsTracker::RequestDoneForGaiaId(const GaiaId& gaia) {
  CHECK(requests_.contains(gaia));
  std::unique_ptr<DeviceStatisticsRequest> request = std::move(requests_[gaia]);
  CHECK(request);
  requests_.erase(gaia);

  if (request->GetState() == DeviceStatisticsRequest::State::kComplete) {
    other_devices_by_gaia_[gaia] =
        GetPlatformsPerDevice(GetRelevantEventsByType(
            request->GetResults(), current_device_cache_guids_));
  } else {
    other_devices_by_gaia_[gaia] = base::unexpected(kRequestFailed);
  }

  if (requests_.empty()) {
    AllRequestsDone();
  }
}

void DeviceStatisticsTracker::AllRequestsDone() {
  CHECK(requests_.empty());
  CHECK(callback_);

  RequestsCompletedSuccess success = GetOverallSuccess();
  base::UmaHistogramEnumeration(
      "Sync.DeviceStatistics.RequestsCompletedSuccess", success);

  if (ShouldRecordOutcomeMetrics(success)) {
    base::UmaHistogramEnumeration("Sync.DeviceStatistics.Outcome.Overall",
                                  GetOverallOutcome());

    for (const auto& [gaia, other_devices] : other_devices_by_gaia_) {
      if (!other_devices.has_value()) {
        continue;
      }

      bool is_primary = (gaia == primary_account_.gaia);
      std::string_view infix =
          is_primary ? "PrimaryAccount" : "NonPrimaryAccount";

      base::UmaHistogramCounts100(
          absl::StrFormat(
              "Sync.DeviceStatistics.Outcome.%s.NumberOfAdditionalClients",
              infix),
          other_devices->size());

      for (Platform platform : *other_devices) {
        base::UmaHistogramEnumeration(
            absl::StrFormat(
                "Sync.DeviceStatistics.Outcome.%s.PlatformOfAdditionalClient",
                infix),
            platform);
      }
    }
  }

  std::move(callback_).Run();
  // NOTE: `this` may be destroyed now; don't do anything else!
}

DeviceStatisticsTracker::RequestsCompletedSuccess
DeviceStatisticsTracker::GetOverallSuccess() const {
  if (primary_account_.gaia !=
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia) {
    return RequestsCompletedSuccess::kPrimaryAccountChangedOrRemoved;
  }

  size_t requests_succeeded = 0;
  size_t requests_failed = 0;
  bool primary_failed = false;
  for (const auto& [gaia, other_devices] : other_devices_by_gaia_) {
    if (other_devices.has_value()) {
      ++requests_succeeded;
    } else {
      ++requests_failed;
      if (gaia == primary_account_.gaia) {
        primary_failed = true;
      }
    }
  }

  if (requests_succeeded == other_devices_by_gaia_.size()) {
    return RequestsCompletedSuccess::kAllSucceeded;
  } else if (requests_failed == other_devices_by_gaia_.size()) {
    return RequestsCompletedSuccess::kAllFailed;
  } else if (primary_failed) {
    return RequestsCompletedSuccess::kPrimaryFailedButNonPrimarySucceeded;
  } else {
    return RequestsCompletedSuccess::kPrimarySucceededButNonPrimaryFailed;
  }
}

DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary
DeviceStatisticsTracker::GetOverallOutcome() const {
  bool primary_account_has_other_devices = false;
  bool non_primary_account_has_other_devices = false;
  for (const auto& [gaia, other_devices] : other_devices_by_gaia_) {
    if (other_devices.has_value() && !other_devices->empty()) {
      if (gaia == primary_account_.gaia) {
        primary_account_has_other_devices = true;
      } else {
        non_primary_account_has_other_devices = true;
      }
    }
  }
  // At least the primary account must always exist.
  CHECK_GE(other_devices_by_gaia_.size(), 1u);
  bool has_non_primary_account = other_devices_by_gaia_.size() > 1;

  if (has_non_primary_account) {
    if (non_primary_account_has_other_devices) {
      return primary_account_has_other_devices
                 ? AccountsHaveOtherDevicesSummary::kPrimaryYesNonPrimaryYes
                 : AccountsHaveOtherDevicesSummary::kPrimaryNoNonPrimaryYes;
    } else {
      return primary_account_has_other_devices
                 ? AccountsHaveOtherDevicesSummary::kPrimaryYesNonPrimaryNo
                 : AccountsHaveOtherDevicesSummary::kPrimaryNoNonPrimaryNo;
    }
  } else {
    return primary_account_has_other_devices
               ? AccountsHaveOtherDevicesSummary::kPrimaryYesNonPrimaryNA
               : AccountsHaveOtherDevicesSummary::kPrimaryNoNonPrimaryNA;
  }
}

}  // namespace syncer
