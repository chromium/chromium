// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_tracker.h"

#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/net/url_translator.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/device_statistics_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

namespace {

constexpr char kLastRecordedPref[] = "sync.device_statistics_timestamp";

std::string GenerateCacheGUID() {
  // Generate a GUID with 128 bits of randomness.
  constexpr int kGuidBytes = 128 / 8;
  return base::Base64Encode(base::RandBytesAsVector(kGuidBytes));
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
    other_devices_by_gaia_[gaia] = std::vector<Platform>();
    // TODO(crbug.com/465716865): Populate `other_devices_by_gaia_[gaia]` from
    // `result->GetResults()`.
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

  // TODO(crbug.com/465716865): Record detailed metrics based on
  // `other_devices_by_gaia_`.

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

}  // namespace syncer
