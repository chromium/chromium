// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_ESIM_POLICY_LOGIN_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_ESIM_POLICY_LOGIN_METRICS_LOGGER_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class ManagedNetworkConfigurationHandler;
class NetworkStateHandler;

// This class adds observers on network state and collects the following eSIM
// policy metrics whenever the user logs in:
// 1. Network.Cellular.ESim.Policy.BlockNonManagedCellularBehavior, i.e:
// whether the admin restricts user from un-managed cellular networks.
// 2. Network.Cellular.ESim.Policy.StatusAtLogin, which measures the adoption
// for policy controlled cellular networks
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ESimPolicyLoginMetricsLogger
    : public NetworkStateHandlerObserver,
      public LoginState::Observer {
 public:
  static const char kESimPolicyBlockNonManagedCellularHistogram[];
  static const char kESimPolicyStatusAtLoginHistogram[];

  // Records the current behavior of block non managed cellular policy if it is
  // different from the last behavior.
  static void RecordBlockNonManagedCellularBehavior(
      bool allow_only_managed_cellular);

  // Represents the behavior of block non managed cellular policy. These values
  // are persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused.
  enum class BlockNonManagedCellularBehavior {
    kAllowManagedOnly = 0,
    kAllowUnmanaged = 1,
    kMaxValue = kAllowUnmanaged
  };

  ESimPolicyLoginMetricsLogger();
  ESimPolicyLoginMetricsLogger(const ESimPolicyLoginMetricsLogger&) = delete;
  ESimPolicyLoginMetricsLogger& operator=(const ESimPolicyLoginMetricsLogger&) =
      delete;
  ~ESimPolicyLoginMetricsLogger() override;

  void Init(NetworkStateHandler* network_state_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler);

  // LoginState::Observer:
  void LoggedInStateChanged() override;

  // NetworkStateHandlerObserver::
  void DeviceListChanged() override;
  void OnShuttingDown() override;

  void SetIsEnterpriseManaged(bool is_enterprise_managed);

 private:
  friend class ESimPolicyLoginMetricsLoggerTest;
  FRIEND_TEST_ALL_PREFIXES(ESimPolicyLoginMetricsLoggerTest, LoginMetricsTest);

  // The amount of time since the cellular device is added to device list after
  // which it is considered initialized.
  static const base::TimeDelta kInitializationTimeout;

  // Tracks the last time the admin allow only connecting to managed cellular
  // network or not.
  static bool last_allow_only_managed_cellular_;

  // Represents the status of whether the eSIM cellular networks contain only
  // managed network, only non-managed network, both or no networks. This status
  // is logged when the user logs in. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should never be reused.
  enum class ESimPolicyStatusAtLogin {
    kNoCellularNetworks = 0,
    kUnmanagedOnly = 1,
    kManagedOnly = 2,
    kManagedAndUnmanaged = 3,
    kMaxValue = kManagedAndUnmanaged
  };

  ESimPolicyStatusAtLogin GetESimPolicyStatusAtLogin(
      bool has_managed_cellular,
      bool has_non_managed_cellular);
  void LogESimPolicyStatusAtLogin();

  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<ManagedNetworkConfigurationHandler, DanglingUntriaged>
      managed_network_configuration_handler_ = nullptr;

  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  // A timer to wait for cellular initialization. This is useful
  // to avoid tracking intermediate states when cellular network is
  // starting up.
  base::OneShotTimer initialization_timer_;

  // Tracks whether the metrics are already logged for this session.
  bool is_metrics_logged_ = false;

  // Tracks whether cellular device is available or not.
  bool is_cellular_available_ = false;

  // Tracks whether ESimPolicyLoginMetricsLogger is initialized or not.
  bool initialized_ = false;

  // Tracks if the device is enterprise managed or not.
  bool is_enterprise_managed_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_METRICS_ESIM_POLICY_LOGIN_METRICS_LOGGER_H_
