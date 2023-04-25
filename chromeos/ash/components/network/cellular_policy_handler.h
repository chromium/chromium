// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_POLICY_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_POLICY_HANDLER_H_

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace ash {

class CellularESimInstaller;
class NetworkProfileHandler;
class NetworkStateHandler;
class ManagedCellularPrefHandler;
class ManagedNetworkConfigurationHandler;
enum class HermesResponseStatus;

// Handles provisioning eSIM profiles via policy.
//
// When installing policy eSIM profiles, the activation code is constructed from
// the SM-DP+ address in the policy configuration. Install requests are queued
// and installation is performed one by one. Install attempts are retried for
// fixed number of tries and the request queue doesn't get blocked by the
// requests that are waiting for retry attempt.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularPolicyHandler
    : public HermesManagerClient::Observer,
      public CellularESimProfileHandler::Observer,
      public NetworkStateHandlerObserver {
 public:
  CellularPolicyHandler();
  CellularPolicyHandler(const CellularPolicyHandler&) = delete;
  CellularPolicyHandler& operator=(const CellularPolicyHandler&) = delete;
  ~CellularPolicyHandler() override;

  void Init(CellularESimProfileHandler* cellular_esim_profile_handler,
            CellularESimInstaller* cellular_esim_installer,
            NetworkProfileHandler* network_profile_handler,
            NetworkStateHandler* network_state_handler,
            ManagedCellularPrefHandler* managed_cellular_pref_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler);

  // Installs an eSIM profile and connects to its network from policy with
  // given |smdp_address|. The Shill service configuration will also be updated
  // to the policy guid and the new ICCID after installation completes. If
  // another eSIM profile is already under installation process, the current
  // request will wait until the previous one is completed. Each installation
  // will be retried for a fixed number of tries.
  void InstallESim(const std::string& smdp_address,
                   const base::Value::Dict& onc_config);

 private:
  // This enum allows us to treat a retry differently depending on what the
  // reason for retrying is.
  enum class InstallRetryReason {
    kMissingNonCellularConnectivity = 0,
    kInternalError = 1,
    kUserError = 2,
    kOther
  };

  // HermesUserErrorCodes indicate errors made by the user. These can be due
  // to bad input or a valid input that has already been successfully processed.
  // In such errors, we do not attempt to retry.
  const std::array<HermesResponseStatus, 4> kHermesUserErrorCodes = {
      HermesResponseStatus::kErrorAlreadyDisabled,
      HermesResponseStatus::kErrorAlreadyEnabled,
      HermesResponseStatus::kErrorInvalidActivationCode,
      HermesResponseStatus::kErrorInvalidIccid};

  // HermesInternalErrorCodes indicate system failure during the installation
  // process. These error can happen due to code bugs or reasons unrelated to
  // user input. In these cases, we retry using an exponental backoff policy to
  // attempt the installation again.
  const std::array<HermesResponseStatus, 7> kHermesInternalErrorCodes = {
      HermesResponseStatus::kErrorUnknown,
      HermesResponseStatus::kErrorInternalLpaFailure,
      HermesResponseStatus::kErrorWrongState,
      HermesResponseStatus::kErrorSendApduFailure,
      HermesResponseStatus::kErrorUnexpectedModemManagerState,
      HermesResponseStatus::kErrorModemMessageProcessing,
      HermesResponseStatus::kErrorPendingProfile};

  friend class CellularPolicyHandlerTest;

  // Represents policy eSIM install request parameters. Requests are queued and
  // processed one at a time. |smdp_address| represents the smdp address that
  // will be used to install the eSIM profile as activation code and
  // |onc_config| is the ONC configuration of the cellular policy.
  struct InstallPolicyESimRequest {
    InstallPolicyESimRequest(const std::string& smdp_address,
                             const base::Value::Dict& onc_config);
    InstallPolicyESimRequest(const InstallPolicyESimRequest&) = delete;
    InstallPolicyESimRequest& operator=(const InstallPolicyESimRequest&) =
        delete;
    ~InstallPolicyESimRequest();

    const std::string smdp_address;
    base::Value::Dict onc_config;
    net::BackoffEntry retry_backoff;
  };

  // HermesManagerClient::Observer:
  void OnAvailableEuiccListChanged() override;

  // CellularESimProfileHandler::Observer:
  void OnESimProfileListUpdated() override;

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;
  void OnShuttingDown() override;

  void ResumeInstallIfNeeded();
  void ProcessRequests();
  void AttemptInstallESim();
  void SetupESim(const dbus::ObjectPath& euicc_path);
  base::Value::Dict GetNewShillProperties();
  const std::string& GetCurrentSmdpAddress() const;
  std::string GetCurrentPolicyGuid() const;
  void OnRefreshProfileList(
      const dbus::ObjectPath& euicc_path,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnConfigureESimService(absl::optional<dbus::ObjectPath> service_path);
  void OnESimProfileInstallAttemptComplete(
      HermesResponseStatus hermes_status,
      absl::optional<dbus::ObjectPath> profile_path,
      absl::optional<std::string> service_path);
  void ScheduleRetry(std::unique_ptr<InstallPolicyESimRequest> request,
                     InstallRetryReason reason);
  void PushRequestAndProcess(std::unique_ptr<InstallPolicyESimRequest> request);
  void PopRequest();
  absl::optional<dbus::ObjectPath> FindExistingMatchingESimProfile();
  void OnWaitTimeout();
  bool HasNonCellularInternetConnectivity();

  raw_ptr<CellularESimProfileHandler, ExperimentalAsh>
      cellular_esim_profile_handler_ = nullptr;
  raw_ptr<CellularESimInstaller, ExperimentalAsh> cellular_esim_installer_ =
      nullptr;
  raw_ptr<NetworkProfileHandler, ExperimentalAsh> network_profile_handler_ =
      nullptr;
  raw_ptr<NetworkStateHandler, ExperimentalAsh> network_state_handler_ =
      nullptr;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  raw_ptr<ManagedCellularPrefHandler, DanglingUntriaged | ExperimentalAsh>
      managed_cellular_pref_handler_ = nullptr;
  raw_ptr<ManagedNetworkConfigurationHandler, ExperimentalAsh>
      managed_network_configuration_handler_ = nullptr;

  bool is_installing_ = false;
  bool need_refresh_profile_list_ = true;
  base::circular_deque<std::unique_ptr<InstallPolicyESimRequest>>
      remaining_install_requests_;

  base::OneShotTimer wait_timer_;

  base::ScopedObservation<HermesManagerClient, HermesManagerClient::Observer>
      hermes_observation_{this};
  base::ScopedObservation<CellularESimProfileHandler,
                          CellularESimProfileHandler::Observer>
      cellular_esim_profile_handler_observation_{this};

  base::WeakPtrFactory<CellularPolicyHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_POLICY_HANDLER_H_
