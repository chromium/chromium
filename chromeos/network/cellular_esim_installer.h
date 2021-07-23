// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_ESIM_INSTALLER_H_
#define CHROMEOS_NETWORK_CELLULAR_ESIM_INSTALLER_H_

#include <map>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/network/cellular_inhibitor.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class CellularConnectionHandler;
class NetworkConnectionHandler;
class NetworkStateHandler;

// Handles installation of an eSIM profile and it's corresponding network.
//
// Installing an eSIM profile involves the following operations:
// 1. Inhibit cellular scans.
// 2. Install eSIM profile in Hermes with activation code.
// 3. Prepare newly installed cellular network for connection (ie. profile
// enable).
// 4. Connect to network with the new profile.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimInstaller {
 public:
  CellularESimInstaller();
  CellularESimInstaller(const CellularESimInstaller&) = delete;
  CellularESimInstaller& operator=(const CellularESimInstaller&) = delete;
  ~CellularESimInstaller();

  void Init(CellularConnectionHandler* cellular_connection_handler,
            CellularInhibitor* cellular_inhibitor,
            NetworkConnectionHandler* network_connection_handler,
            NetworkStateHandler* network_state_handler);

  using InstallProfileFromActivationCodeCallback =
      base::OnceCallback<void(HermesResponseStatus,
                              absl::optional<dbus::ObjectPath>)>;

  // Installs an ESim profile and network with given |activation_code|,
  // |confirmation_code| and |euicc_path|. This method will also attempt
  // to enable the newly installed profile and connect to its network.
  void InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      const dbus::ObjectPath& euicc_path,
      InstallProfileFromActivationCodeCallback callback);

 private:
  friend class CellularESimInstallerTest;
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest,
                           InstallProfileInvalidActivationCode);
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest,
                           InstallProfileConnectFailure);
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest, InstallProfileSuccess);
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest,
                           InstallProfileAlreadyConnected);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class InstallProfileViaQrCodeResult {
    kSuccess = 0,
    kInhibitFailed = 1,
    kHermesInstallFailed = 2,
    kMaxValue = kHermesInstallFailed
  };
  static void RecordInstallProfileViaQrCodeResult(
      InstallProfileViaQrCodeResult result);

  void PerformInstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      const dbus::ObjectPath& euicc_path,
      InstallProfileFromActivationCodeCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnProfileInstallResult(
      InstallProfileFromActivationCodeCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      const dbus::ObjectPath& euicc_path,
      HermesResponseStatus status,
      const dbus::ObjectPath* object_path);
  void OnPrepareCellularNetworkForConnectionSuccess(
      const dbus::ObjectPath& profile_path,
      const std::string& service_path);
  void OnPrepareCellularNetworkForConnectionFailure(
      const dbus::ObjectPath& profile_path,
      const std::string& service_path,
      const std::string& error_name);
  void HandleNewProfileEnableFailure(const dbus::ObjectPath& profile_path,
                                     const std::string& error_name);

  CellularConnectionHandler* cellular_connection_handler_;
  CellularInhibitor* cellular_inhibitor_;

  NetworkConnectionHandler* network_connection_handler_;
  NetworkStateHandler* network_state_handler_;

  // Maps profile dbus paths to InstallProfileFromActivation method callbacks
  // that are pending connection to the newly created network.
  std::map<dbus::ObjectPath, InstallProfileFromActivationCodeCallback>
      install_calls_pending_connect_;

  base::WeakPtrFactory<CellularESimInstaller> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_ESIM_INSTALLER_H_