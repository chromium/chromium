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
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
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
    case RequestsCompletedSuccess::kPrimaryNAAndSomeNonPrimaryFailed:
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

bool IsOptedInToHistory(const sync_pb::DeviceInfoSpecifics device_info) {
  // Check whether the device interested in history invalidations. Note that
  // it's better to check for `DataType::HISTORY_DELETE_DIRECTIVES` rather than
  // `DataType::HISTORY`, since Android devices are generally not subscribed
  // to `DataType::HISTORY`.
  for (int data_type_number :
       device_info.invalidation_fields().interested_data_type_ids()) {
    if (GetDataTypeFromSpecificsFieldNumber(data_type_number) ==
        DataType::HISTORY_DELETE_DIRECTIVES) {
      return true;
    }
  }
  return false;
}

}  // namespace

DeviceStatisticsTracker::DeviceStatisticsTracker(
    signin::IdentityManager* identity_manager,
    const GURL& sync_server_url,
    RequestFactory request_factory,
    std::vector<std::string> current_device_cache_guids)
    : identity_manager_(identity_manager),
      sync_server_url_(sync_server_url),
      request_factory_(std::move(request_factory)),
      current_device_cache_guids_(std::move(current_device_cache_guids)),
      primary_account_(identity_manager_->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin)) {
  CHECK(identity_manager_);
  CHECK(request_factory_);
}

void DeviceStatisticsTracker::Start(base::OnceClosure callback) {
  CHECK(!callback_);
  CHECK(callback);
  CHECK(identity_manager_->AreRefreshTokensLoaded());

  callback_ = std::move(callback);

  const std::vector<CoreAccountInfo> accounts =
      identity_manager_->GetAccountsWithRefreshTokens();

  if (!primary_account_.IsEmpty() &&
      !std::ranges::contains(accounts, primary_account_)) {
    // The primary account must have been removed between the constructor and
    // now, or something's wrong with the IdentityManager.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback_));
    return;
  }

  // If there are no accounts, there's not much to do.
  if (accounts.empty()) {
    RecordOverallOutcome();
    RecordPrimaryAccountMultiDeviceReadiness(
        /*other_devices=*/0, /*other_devices_with_history_opt_in=*/0);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback_));
    return;
  }

  GURL::Replacements path_replacement;
  std::string path = sync_server_url_.GetPath() + "/command/";
  path_replacement.SetPathStr(path);
  GURL base_url = sync_server_url_.ReplaceComponents(path_replacement);

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

void DeviceStatisticsTracker::RequestDoneForGaiaId(const GaiaId& gaia) {
  CHECK(requests_.contains(gaia));
  std::unique_ptr<DeviceStatisticsRequest> request = std::move(requests_[gaia]);
  CHECK(request);
  requests_.erase(gaia);

  if (request->GetState() == DeviceStatisticsRequest::State::kComplete) {
    other_devices_by_gaia_[gaia] =
        DeduplicateEntities(request->GetResults(), current_device_cache_guids_);

    // If this request was for the primary account, figure out whether the user
    // has opted in to history on the current device.
    if (gaia == primary_account_.gaia) {
      for (const sync_pb::SyncEntity& entity : request->GetResults()) {
        const sync_pb::DeviceInfoSpecifics& device_info =
            entity.specifics().device_info();
        // Note: `current_device_cache_guids_` contains all known cache GUIDs
        // for the current device, including some that may belong to other
        // accounts. But since `device_info` belongs to the primary account, and
        // cache GUIDs should be unique (and in particular, never shared across
        // different accounts), that's okay.
        if (current_device_cache_guids_.contains(device_info.cache_guid()) &&
            IsOptedInToHistory(device_info)) {
          primary_account_history_opt_in_ = true;
          break;
        }
      }
    }
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
    RecordOverallOutcome();

    for (const auto& [gaia, other_devices] : other_devices_by_gaia_) {
      if (!other_devices.has_value()) {
        // The statistics request for this account failed, so nothing to record.
        continue;
      }

      const bool is_primary = (gaia == primary_account_.gaia);
      const std::string_view infix =
          is_primary ? "PrimaryAccount" : "NonPrimaryAccount";

      base::UmaHistogramCounts100(
          absl::StrFormat(
              "Sync.DeviceStatistics.Outcome.%s.NumberOfAdditionalClients",
              infix),
          other_devices->size());

      const size_t other_devices_with_history_opt_in = std::ranges::count_if(
          *other_devices,
          [](const DeviceData& device) { return device.history_opt_in; });
      base::UmaHistogramCounts100(
          absl::StrFormat("Sync.DeviceStatistics.Outcome.%s."
                          "NumberOfAdditionalClientsWithHistoryOptIn",
                          infix),
          other_devices_with_history_opt_in);

      if (is_primary) {
        RecordPrimaryAccountMultiDeviceReadiness(
            other_devices->size(), other_devices_with_history_opt_in);

        base::UmaHistogramEnumeration(
            "Sync.DeviceStatistics.Outcome.PrimaryAccount.HistoryOptIn",
            GetHistoryOptInSummary(other_devices->size(),
                                   other_devices_with_history_opt_in));
      }

      for (DeviceData device : *other_devices) {
        base::UmaHistogramEnumeration(
            absl::StrFormat(
                "Sync.DeviceStatistics.Outcome.%s.PlatformOfAdditionalClient",
                infix),
            device.platform);
      }
    }
  }

  std::move(callback_).Run();
  // NOTE: `this` may be destroyed now; don't do anything else!
}

void DeviceStatisticsTracker::RecordOverallOutcome() const {
  base::UmaHistogramEnumeration("Sync.DeviceStatistics.Outcome.Overall",
                                GetOverallOutcome());
}

void DeviceStatisticsTracker::RecordPrimaryAccountMultiDeviceReadiness(
    size_t other_devices,
    size_t other_devices_with_history_opt_in) const {
  base::UmaHistogramEnumeration(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiDeviceReadiness",
      GetPrimaryAccountMultiDeviceReadiness(other_devices,
                                            other_devices_with_history_opt_in));
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

  // TODO(crbug.com/465716865): Consider treating some types of errors
  // specially, e.g disabled-by-admin.

  if (requests_succeeded == other_devices_by_gaia_.size()) {
    return RequestsCompletedSuccess::kAllSucceeded;
  } else if (requests_failed == other_devices_by_gaia_.size()) {
    return RequestsCompletedSuccess::kAllFailed;
  } else if (primary_account_.IsEmpty()) {
    return RequestsCompletedSuccess::kPrimaryNAAndSomeNonPrimaryFailed;
  } else if (primary_failed) {
    return RequestsCompletedSuccess::kPrimaryFailedButNonPrimarySucceeded;
  } else {
    return RequestsCompletedSuccess::kPrimarySucceededButNonPrimaryFailed;
  }
}

DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary
DeviceStatisticsTracker::GetOverallOutcome() const {
  if (other_devices_by_gaia_.empty()) {
    return AccountsHaveOtherDevicesSummary::kNoAccounts;
  }

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

  if (!primary_account_.IsEmpty()) {
    CHECK(other_devices_by_gaia_.contains(primary_account_.gaia));
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
  } else {
    if (non_primary_account_has_other_devices) {
      return AccountsHaveOtherDevicesSummary::kPrimaryNANonPrimaryYes;
    } else {
      return AccountsHaveOtherDevicesSummary::kPrimaryNANonPrimaryNo;
    }
  }
}

DeviceStatisticsTracker::MultiDeviceReadiness
DeviceStatisticsTracker::GetPrimaryAccountMultiDeviceReadiness(
    size_t other_devices,
    size_t other_devices_with_history_opt_in) const {
  if (primary_account_.IsEmpty()) {
    return MultiDeviceReadiness::kSignedOut;
  }
  if (other_devices == 0) {
    return MultiDeviceReadiness::kSingleDevice;
  }
  if (!primary_account_history_opt_in_ ||
      other_devices_with_history_opt_in == 0) {
    return MultiDeviceReadiness::kMultiDeviceWithoutHistory;
  }
  return MultiDeviceReadiness::kMultiDeviceWithHistory;
}

DeviceStatisticsTracker::HistoryOptInSummary
DeviceStatisticsTracker::GetHistoryOptInSummary(
    size_t other_devices,
    size_t other_devices_with_history_opt_in) const {
  if (other_devices == 0) {
    if (primary_account_history_opt_in_) {
      return HistoryOptInSummary::kThisDeviceYesOtherDevicesNA;
    }
    return HistoryOptInSummary::kThisDeviceNoOtherDevicesNA;
  }

  if (other_devices_with_history_opt_in > 0) {
    if (primary_account_history_opt_in_) {
      return HistoryOptInSummary::kThisDeviceYesOtherDevicesYes;
    }
    return HistoryOptInSummary::kThisDeviceNoOtherDevicesYes;
  }

  if (primary_account_history_opt_in_) {
    return HistoryOptInSummary::kThisDeviceYesOtherDevicesNo;
  }
  return HistoryOptInSummary::kThisDeviceNoOtherDevicesNo;
}

// static
std::vector<DeviceStatisticsTracker::DeviceData>
DeviceStatisticsTracker::DeduplicateEntities(
    const std::vector<sync_pb::SyncEntity>& entities,
    const base::flat_set<std::string>& current_device_cache_guids) {
  absl::flat_hash_map<std::pair<sync_pb::SyncEnums::DeviceFormFactor,
                                sync_pb::SyncEnums::OsType>,
                      std::vector<sync_pb::SyncEntity>>
      devices_by_type;

  const base::Time now = base::Time::Now();

  // Group all the relevant DeviceInfos by OS+FormFactor.
  for (const sync_pb::SyncEntity& entity : entities) {
    const sync_pb::DeviceInfoSpecifics& device =
        entity.specifics().device_info();
    // Only consider Chrome devices (not Google Play Services).
    if (!device.has_chrome_version_info()) {
      continue;
    }

    // Don't consider the current device.
    if (current_device_cache_guids.contains(device.cache_guid())) {
      continue;
    }

    // Only consider recently-used devices.
    base::Time last_updated_time =
        ProtoTimeToTime(device.last_updated_timestamp());
    if (now - last_updated_time > kDeviceActivityTimeRange) {
      continue;
    }

    // This is a relevant device!
    sync_pb::SyncEnums::DeviceFormFactor form_factor =
        device.device_form_factor();
    sync_pb::SyncEnums::OsType os_type = device.os_type();
    devices_by_type[{form_factor, os_type}].push_back(entity);
  }

  std::vector<DeviceData> deduped_devices;

  // Heuristically de-dupe entities based on their activity time ranges (similar
  // in spirit to DeviceInfoSyncBridge::CountActiveDevicesByType()): If two
  // entries have non-overlapping activity times (ctime/mtime), they most likely
  // represent the same device, just with different cache GUIDs.
  // As an approximation, to avoid an O(n^2) algorithm, just de-dupe all entries
  // against the last-created one.
  for (auto& [form_factor_and_os_type, type_entities] : devices_by_type) {
    CHECK(!type_entities.empty());
    int64_t max_ctime = std::ranges::max(type_entities, /*comp=*/{},
                                         [](const sync_pb::SyncEntity& entity) {
                                           return entity.ctime();
                                         })
                            .ctime();
    for (const sync_pb::SyncEntity& entity : type_entities) {
      if (entity.mtime() < max_ctime) {
        // This entity was last used before the newest one was created. That
        // means it's likely the same device, just with a different cache GUID.
        continue;
      }

      // Figure out the platform/OS of the device, and skip unknown or
      // uninteresting ones.
      std::optional<DeviceStatisticsTracker::Platform> platform =
          PlatformFromProto(entity.specifics().device_info().os_type());
      if (!platform) {
        continue;
      }

      // Figure out whether the device has opted in to history.
      bool history_opt_in =
          IsOptedInToHistory(entity.specifics().device_info());

      deduped_devices.emplace_back(*platform, history_opt_in);
    }
  }

  return deduped_devices;
}

}  // namespace syncer
