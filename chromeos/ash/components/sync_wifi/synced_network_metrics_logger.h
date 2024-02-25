// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_METRICS_LOGGER_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_connection_observer.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/sync_wifi/network_eligibility_checker.h"

namespace ash {

class NetworkConnectionHandler;
class NetworkState;
class NetworkStateHandler;

namespace sync_wifi {

const char kConnectionFailureReasonAllHistogram[] =
    "Network.Wifi.Synced.Connection.FailureReason";
const char kConnectionResultAllHistogram[] =
    "Network.Wifi.Synced.Connection.Result";

const char kConnectionFailureReasonManualHistogram[] =
    "Network.Wifi.Synced.ManualConnection.FailureReason";
const char kConnectionResultManualHistogram[] =
    "Network.Wifi.Synced.ManualConnection.Result";

const char kApplyFailureReasonHistogram[] =
    "Network.Wifi.Synced.UpdateOperation.FailureReason";
const char kApplyGenerateLocalNetworkConfigHistogram[] =
    "Network.Wifi.Synced.UpdateOperation.GenerateLocalNetworkConfig.Result";
const char kApplyResultHistogram[] =
    "Network.Wifi.Synced.UpdateOperation.Result";
const char kZeroNetworksSyncedReasonHistogram[] =
    "Network.Wifi.Synced.ZeroNetworksEligibleForSync.Reason";

const char kTotalCountHistogram[] = "Network.Wifi.Synced.TotalCount";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ConnectionFailureReason {
  kUnknownDeprecated = 0,  // deprecated
  kBadPassphrase = 1,
  kBadWepKey = 2,
  kFailedToConnect = 3,
  kDhcpFailure = 4,
  kDnsLookupFailure = 5,
  kEapAuthentication = 6,
  kEapLocalTls = 7,
  kEapRemoteTls = 8,
  kOutOfRange = 9,
  kPinMissing = 10,
  kUnknown = 11,
  kNoFailure = 12,
  kNotAssociated = 13,
  kNotAuthenticated = 14,
  kTooManySTAs = 15,
  kMaxValue = kTooManySTAs
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ApplyNetworkFailureReason {
  kUnknown = 0,
  kAlreadyExists = 1,
  kFailedToRemove = 2,
  kFailedToUpdate = 3,
  kFailedToAdd = 4,
  kInProgress = 5,
  kInternalError = 6,
  kInvalidArguments = 7,
  kInvalidNetworkName = 8,
  kInvalidPassphrase = 9,
  kInvalidProperty = 10,
  kNotSupported = 11,
  kPassphraseRequired = 12,
  kPermissionDenied = 13,
  kTimedout = 14,
  kMaxValue = kTimedout
};

// Logs connection metrics for networks which were configured by sync.
class SyncedNetworkMetricsLogger : public NetworkConnectionObserver,
                                   public NetworkStateHandlerObserver {
 public:
  SyncedNetworkMetricsLogger(
      NetworkStateHandler* network_state_handler,
      NetworkConnectionHandler* network_connection_handler);
  ~SyncedNetworkMetricsLogger() override;

  SyncedNetworkMetricsLogger(const SyncedNetworkMetricsLogger&) = delete;
  SyncedNetworkMetricsLogger& operator=(const SyncedNetworkMetricsLogger&) =
      delete;

  // NetworkConnectionObserver::
  void ConnectSucceeded(const std::string& service_path) override;
  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override;

  // NetworkStateObserver::
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // Only record after all retries have failed.
  void RecordApplyNetworkFailed();
  // Record the cause of failure for all tries.  |error_string| is optional,
  // |error_enum| will be used unless the provided |error_string| can be mapped
  // to a more specific error.
  void RecordApplyNetworkFailureReason(ApplyNetworkFailureReason error_enum,
                                       const std::string& error_string);
  void RecordApplyNetworkSuccess();
  void RecordApplyGenerateLocalNetworkConfig(bool success);
  void RecordTotalCount(int count);
  void RecordZeroNetworksEligibleForSync(
      base::flat_set<NetworkEligibilityStatus>
          network_eligiblility_status_codes);

 private:
  static ConnectionFailureReason ConnectionFailureReasonToEnum(
      const std::string& reason);
  static ApplyNetworkFailureReason ApplyFailureReasonToEnum(
      const std::string& reason);

  void OnConnectErrorGetProperties(
      const std::string& error_name,
      const std::string& service_path,
      std::optional<base::Value::Dict> shill_properties);

  bool IsEligible(const NetworkState* network);

  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<NetworkConnectionHandler> network_connection_handler_ = nullptr;

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  // Contains the guids of networks which are currently connecting.
  base::flat_set<std::string> connecting_guids_;

  // The timestamp when the constructor was executed.
  base::Time initialized_timestamp_;

  base::WeakPtrFactory<SyncedNetworkMetricsLogger> weak_ptr_factory_{this};
};

}  // namespace sync_wifi

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_METRICS_LOGGER_H_
