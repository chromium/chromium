// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_DEFAULT_NETWORK_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_DEFAULT_NETWORK_METRICS_LOGGER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class NetworkState;
class NetworkStateHandler;

// Class for tracking info about the default network.
//
// This class adds observers on network state and currently makes the
// following measurements on the default networks:
//
// 1. The possible metered and non-metered technology type of the first
//    active/default network that is non-virtual when the active/default
//    network changes or when the metered property changes. Note that if
//    the default/active network drops and then reconnects without another
//    network taking its place, this metric will not be emitted.
//
// Note: This class does not start logging metrics until Init() is
// invoked.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) DefaultNetworkMetricsLogger
    : public NetworkStateHandlerObserver {
 public:
  DefaultNetworkMetricsLogger();
  DefaultNetworkMetricsLogger(const DefaultNetworkMetricsLogger&) = delete;
  DefaultNetworkMetricsLogger& operator=(const DefaultNetworkMetricsLogger&) =
      delete;
  ~DefaultNetworkMetricsLogger() override;

  void Init(NetworkStateHandler* network_state_handler);

 private:
  friend class DefaultNetworkMetricsLoggerTest;
  FRIEND_TEST_ALL_PREFIXES(DefaultNetworkMetricsLoggerTest,
                           NetworkTechnologyMeterSubtypeChanges);

  // Histograms associated with SIM Pin operations.
  static const char kDefaultNetworkMeterSubtypeHistogram[];

  // Corresponds to NetworkTechnologyMeterSubtype in network/enums.xml. These
  // values are persisted to logs. Entries should not be renumbered and numeric
  // values should never be reused.
  enum class NetworkTechnologyMeterSubtype {
    kEthernet = 0,
    kWifi = 1,
    kWifiMetered = 2,
    kCellular = 3,
    kCellularMetered = 4,
    kTether = 5,
    kTetherMetered = 6,
    kMaxValue = kTetherMetered
  };

  // NetworkStateHandlerObserver::
  void DefaultNetworkChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  void UpdateAndRecordTechnologyMeterSubtype();

  std::optional<NetworkTechnologyMeterSubtype> GetNetworkTechnologyMeterSubtype(
      const NetworkState* network);

  std::string guid_;
  bool is_metered_;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_DEFAULT_NETWORK_METRICS_LOGGER_H_
