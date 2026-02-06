// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_TRACKER_H_
#define COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_TRACKER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_id.h"
#include "url/gurl.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace sync_pb {
class SyncEntity;
}  // namespace sync_pb

namespace syncer {

class DeviceStatisticsRequest;

// This class sends a DeviceStatisticsRequest for every account on the device,
// and once they all complete, records metrics about this.
class DeviceStatisticsTracker {
 public:
  using RequestFactory = base::RepeatingCallback<std::unique_ptr<
      DeviceStatisticsRequest>(const CoreAccountInfo&, const GURL&)>;

  // `identity_manager` must not be null.
  DeviceStatisticsTracker(signin::IdentityManager* identity_manager,
                          const GURL& sync_server_url,
                          RequestFactory request_factory,
                          std::vector<std::string> current_device_cache_guids);
  ~DeviceStatisticsTracker();

  // If there was no previous invocation today, sends `DeviceStatisticsRequest`s
  // for all accounts. Once they all complete, records metrics and then runs the
  // `callback`. If `this` is destroyed, `callback` is not run anymore.
  // Must only be called after refresh tokens have been loaded (so that the list
  // of accounts can actually be queried).
  void Start(base::OnceClosure callback);

  // Metrics enums are public for testing.

  // LINT.IfChange(SyncDeviceStatisticsSuccess)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RequestsCompletedSuccess {
    // Overall successful, metrics will be recorded:
    kAllSucceeded = 0,
    kPrimarySucceededButNonPrimaryFailed = 1,
    // Overall failed, no further metrics will be recorded:
    kPrimaryFailedButNonPrimarySucceeded = 2,
    kAllFailed = 3,
    kPrimaryAccountChangedOrRemoved = 4,
    kPrimaryNAAndSomeNonPrimaryFailed = 5,
    kMaxValue = kPrimaryNAAndSomeNonPrimaryFailed
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sync/enums.xml:SyncDeviceStatisticsSuccess)

  // LINT.IfChange(SyncDeviceStatisticsOutcome)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AccountsHaveOtherDevicesSummary {
    kPrimaryYesNonPrimaryNA = 0,
    kPrimaryNoNonPrimaryNA = 1,
    kPrimaryYesNonPrimaryYes = 2,
    kPrimaryYesNonPrimaryNo = 3,
    kPrimaryNoNonPrimaryYes = 4,
    kPrimaryNoNonPrimaryNo = 5,
    kPrimaryNANonPrimaryYes = 6,
    kPrimaryNANonPrimaryNo = 7,
    kNoAccounts = 8,
    kMaxValue = kNoAccounts
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sync/enums.xml:SyncDeviceStatisticsOutcome)

  // LINT.IfChange(SyncDeviceStatisticsMultiDeviceReadiness)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class MultiDeviceReadiness {
    // There is no primary account on the current device.
    kSignedOut = 0,
    // There is a primary account on the current device, but no other devices.
    kSingleDevice = 1,
    // There are other devices, but there is no history opt-in across this
    // device and others (i.e. this device doesn't have history, and/or no
    // other devices do).
    kMultiDeviceWithoutHistory = 2,
    // There are other devices, and both this device plus at least one other
    // device have history opt-in.
    kMultiDeviceWithHistory = 3,
    kMaxValue = kMultiDeviceWithHistory
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sync/enums.xml:SyncDeviceStatisticsMultiDeviceReadiness)

  // LINT.IfChange(SyncDeviceStatisticsPlatform)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Platform {
    kWindows = 0,
    kMac = 1,
    kLinux = 2,
    kChromeOS = 3,
    kAndroid = 4,
    kIOS = 5,
    kMaxValue = kIOS
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sync/enums.xml:SyncDeviceStatisticsPlatform)

  // LINT.IfChange(SyncDeviceStatisticsHistoryOptInSummary)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class HistoryOptInSummary {
    kThisDeviceYesOtherDevicesNA = 0,
    kThisDeviceNoOtherDevicesNA = 1,
    kThisDeviceYesOtherDevicesYes = 2,
    kThisDeviceYesOtherDevicesNo = 3,
    kThisDeviceNoOtherDevicesYes = 4,
    kThisDeviceNoOtherDevicesNo = 5,
    kMaxValue = kThisDeviceNoOtherDevicesNo
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sync/enums.xml:SyncDeviceStatisticsHistoryOptInSummary)

 private:
  struct DeviceData {
    DeviceData(Platform platform, bool history_opt_in)
        : platform(platform), history_opt_in(history_opt_in) {}
    ~DeviceData() = default;

    const Platform platform;
    const bool history_opt_in;
  };

  void RequestDoneForGaiaId(const GaiaId& gaia);
  void AllRequestsDone();

  void RecordOverallOutcome() const;
  // Records `MultiDeviceReadiness` for the primary account (including for the
  // case when there is no primary account).
  void RecordPrimaryAccountMultiDeviceReadiness(
      size_t other_devices,
      size_t other_devices_with_history_opt_in) const;

  RequestsCompletedSuccess GetOverallSuccess() const;
  AccountsHaveOtherDevicesSummary GetOverallOutcome() const;
  MultiDeviceReadiness GetPrimaryAccountMultiDeviceReadiness(
      size_t other_devices,
      size_t other_devices_with_history_opt_in) const;
  HistoryOptInSummary GetHistoryOptInSummary(
      size_t other_devices,
      size_t other_devices_with_history_opt_in) const;

  std::vector<DeviceData> DeduplicateEntities(
      const std::vector<sync_pb::SyncEntity>& entities,
      const base::flat_set<std::string>& current_device_cache_guids);

  const raw_ptr<signin::IdentityManager> identity_manager_;

  const GURL sync_server_url_;

  const RequestFactory request_factory_;

  const base::flat_set<std::string> current_device_cache_guids_;

  // Cached on construction, to later verify it hasn't changed. May be empty.
  const CoreAccountInfo primary_account_;

  // Whether the primary account is opted in to history (on this device).
  // Populated when the corresponding request completes.
  bool primary_account_history_opt_in_ = false;

  base::OnceClosure callback_;

  // Ongoing requests.
  base::flat_map<GaiaId, std::unique_ptr<DeviceStatisticsRequest>> requests_;

  class RequestFailed {};
  const RequestFailed kRequestFailed;

  // Results. This will have exactly one entry for every request that completed,
  // successfully or not. On success, the value is the list of all valid other
  // devices (which may be empty).
  base::flat_map<GaiaId, base::expected<std::vector<DeviceData>, RequestFailed>>
      other_devices_by_gaia_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_TRACKER_H_
