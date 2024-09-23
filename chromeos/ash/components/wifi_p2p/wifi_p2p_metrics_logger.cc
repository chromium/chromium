// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/wifi_p2p/wifi_p2p_metrics_logger.h"

#include "base/metrics/histogram_functions.h"

namespace ash {

using OperationResult = WifiP2PController::OperationResult;
using OperationType = WifiP2PController::OperationType;
using WifiP2PCapabilities = WifiP2PController::WifiP2PCapabilities;

// static
const char WifiP2PMetricsLogger::kWifiP2PCapabilitiesHistogram[] =
    "Network.Ash.WiFiDirect.CapabilitiesWhenUpgrade";

// static
const char WifiP2PMetricsLogger::kCreateP2PGroupHistogram[] =
    "Network.Ash.WiFiDirect.CreateP2PGroup.OperationResult";

// static
const char WifiP2PMetricsLogger::kConnectP2PGroupHistogram[] =
    "Network.Ash.WiFiDirect.ConnectP2PGroup.OperationResult";

// static
const char WifiP2PMetricsLogger::kDisconnectP2PGroupHistogram[] =
    "Network.Ash.WiFiDirect.DisconnectP2PGroup.OperationResult";

// static
const char WifiP2PMetricsLogger::kDestroyP2PGroupHistogram[] =
    "Network.Ash.WiFiDirect.DestroyP2PGroup.OperationResult";

// static
const char WifiP2PMetricsLogger::kTagSocketHistogram[] =
    "Network.Ash.WiFiDirect.TagSocket.OperationResult";

// static
const char WifiP2PMetricsLogger::kWifiP2PConnectionDurationHistogram[] =
    "Network.Ash.WiFiDirect.Connection.Duration";

// static
const char WifiP2PMetricsLogger::kGroupOwnerDisconnectReasonHistogram[] =
    "Network.Ash.WiFiDirect.GroupOwner.DisconnectReason";

// static
const char WifiP2PMetricsLogger::kGroupClientDisconnectReasonHistogram[] =
    "Network.Ash.WiFiDirect.GroupClient.DisconnectReason";

// static
void WifiP2PMetricsLogger::RecordWifiP2PCapabilities(
    const WifiP2PCapabilities& capablities) {
  base::UmaHistogramEnumeration(kWifiP2PCapabilitiesHistogram,
                                GetWifiP2PMetricsCapabilities(capablities));
}

// static
void WifiP2PMetricsLogger::RecordWifiP2POperationResult(
    const OperationType& type,
    const OperationResult& result) {
  switch (type) {
    case OperationType::kCreateGroup:
      base::UmaHistogramEnumeration(kCreateP2PGroupHistogram, result);
      return;
    case OperationType::kConnectGroup:
      base::UmaHistogramEnumeration(kConnectP2PGroupHistogram, result);
      return;
    case OperationType::kDestroyGroup:
      base::UmaHistogramEnumeration(kDestroyP2PGroupHistogram, result);
      return;
    case OperationType::kDisconnectGroup:
      base::UmaHistogramEnumeration(kDisconnectP2PGroupHistogram, result);
      return;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown WiFi P2P operation type: " << type;
}

void WifiP2PMetricsLogger::RecordTagSocketOperationResult(bool success) {
  base::UmaHistogramBoolean(kTagSocketHistogram, success);
}

void WifiP2PMetricsLogger::RecordWifiP2PConnectionDuration(
    const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes(kWifiP2PConnectionDurationHistogram, duration);
}

void WifiP2PMetricsLogger::RecordWifiP2PDisconnectReason(
    DisconnectReason reason,
    bool is_owner) {
  base::UmaHistogramEnumeration(is_owner
                                    ? kGroupOwnerDisconnectReasonHistogram
                                    : kGroupClientDisconnectReasonHistogram,
                                reason);
}

// static
WifiP2PMetricsLogger::WifiP2PMetricsCapabilities
WifiP2PMetricsLogger::GetWifiP2PMetricsCapabilities(
    const WifiP2PCapabilities& capabilities) {
  if (capabilities.is_client_ready && capabilities.is_owner_ready) {
    return WifiP2PMetricsCapabilities::kBothClientAndOwnerReady;
  }
  if (capabilities.is_client_ready) {
    return WifiP2PMetricsCapabilities::kOnlyClientReady;
  }
  if (capabilities.is_owner_ready) {
    return WifiP2PMetricsCapabilities::kOnlyOwnerReady;
  }
  return WifiP2PMetricsCapabilities::kNeitherClientNorOwnerReady;
}

}  // namespace ash
