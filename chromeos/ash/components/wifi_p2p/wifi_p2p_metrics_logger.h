// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_METRICS_LOGGER_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"

namespace ash {

// This class is used to track the WiFi P2P capabilities and operation result
// and emits UMA metrics to the related histogram.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_WIFI_P2P) WifiP2PMetricsLogger {
 public:
  // Represents the Wifi P2P disconnect reason. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class DisconnectReason {
    kClientInitiated = 0,
    kInternalError = 1,
    kMaxValue = kInternalError,
  };

  // Emits whenever client queries the Wifi P2P capabilities.
  static void RecordWifiP2PCapabilities(
      const WifiP2PController::WifiP2PCapabilities& capablities);

  // Emits whenever a Wifi P2P operation is attempted.
  static void RecordWifiP2POperationResult(
      const WifiP2PController::OperationType& type,
      const WifiP2PController::OperationResult& result);

  // Emits whenever a tag socket operation is attempted.
  static void RecordTagSocketOperationResult(bool success);

  // Emits when the Wifi P2P connection is finished.
  static void RecordWifiP2PConnectionDuration(const base::TimeDelta& duration);

  // Emit when the Wifi P2P connection disconnects.
  static void RecordWifiP2PDisconnectReason(DisconnectReason reason,
                                            bool is_owner);

  WifiP2PMetricsLogger() = default;
  ~WifiP2PMetricsLogger() = default;
  WifiP2PMetricsLogger(const WifiP2PMetricsLogger&) = delete;
  WifiP2PMetricsLogger& operator=(const WifiP2PMetricsLogger&) = delete;

 private:
  friend class WifiP2PControllerTest;
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest,
                           CreateP2PGroupWithCredentials_Success);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest,
                           CreateP2PGroupWithoutCredentials_Success);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest,
                           CreateP2PGroupFailure_InvalidArguments);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest,
                           CreateP2PGroupFailure_DBusError);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest, DestroyP2PGroupSuccess);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest,
                           DestroyP2PGroupSuccess_GroupNotFound);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest, ConnectToP2PGroupSuccess);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest,
                           ConnectToP2PGroupFailure_ConcurrencyNotSupported);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest,
                           DisconnectFromP2PGroupSuccess);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest,
                           DisconnectFromP2PGroupFailure_NotConnected);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest, GetP2PCapabilities);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest, TagSocketSuccess);
  FRIEND_TEST_ALL_PREFIXES(WifiP2PControllerTest, TagSocketFailure);

  // Represents the Wifi P2P capabilities for the metric. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class WifiP2PMetricsCapabilities {
    kNeitherClientNorOwnerReady = 0,
    kOnlyOwnerReady = 1,
    kOnlyClientReady = 2,
    kBothClientAndOwnerReady = 3,
    kMaxValue = kBothClientAndOwnerReady,
  };

  static const char kWifiP2PCapabilitiesHistogram[];
  static const char kCreateP2PGroupHistogram[];
  static const char kConnectP2PGroupHistogram[];
  static const char kDisconnectP2PGroupHistogram[];
  static const char kDestroyP2PGroupHistogram[];
  static const char kTagSocketHistogram[];
  static const char kWifiP2PConnectionDurationHistogram[];
  static const char kGroupOwnerDisconnectReasonHistogram[];
  static const char kGroupClientDisconnectReasonHistogram[];

  static WifiP2PMetricsCapabilities GetWifiP2PMetricsCapabilities(
      const WifiP2PController::WifiP2PCapabilities& capablities);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_WIFI_P2P_WIFI_P2P_METRICS_LOGGER_H_
