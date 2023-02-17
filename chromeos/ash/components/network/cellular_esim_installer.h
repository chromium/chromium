// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_INSTALLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_INSTALLER_H_

#include <map>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace ash {

class CellularConnectionHandler;
class NetworkConnectionHandler;
class NetworkProfileHandler;
class NetworkStateHandler;

// Handles installation of an eSIM profile and it's corresponding network.
//
// Installing an eSIM profile involves the following operations:
// 1. Inhibit cellular scans.
// 2. Install eSIM profile in Hermes with activation code.
// 3. Create cellular Shill service configuration.
// 4. Prepare newly installed cellular network for connection (ie. profile
// enable).
// 5. Connect to network with the new profile.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimInstaller {
 public:
  CellularESimInstaller();
  CellularESimInstaller(const CellularESimInstaller&) = delete;
  CellularESimInstaller& operator=(const CellularESimInstaller&) = delete;
  ~CellularESimInstaller();

  void Init(CellularConnectionHandler* cellular_connection_handler,
            CellularInhibitor* cellular_inhibitor,
            NetworkConnectionHandler* network_connection_handler,
            NetworkProfileHandler* network_profile_handler,
            NetworkStateHandler* network_state_handler);

  // Return callback for the InstallProfileFromActivationCode method.
  // |hermes_status| is the status of the eSIM installation.
  // |profile_path| is the path to the newly installed eSIM profile
  // and |service_path| is the path to the corresponding network service.
  // |profile_path| and |service_path| will be absl::nullopt on error.
  using InstallProfileFromActivationCodeCallback =
      base::OnceCallback<void(HermesResponseStatus hermes_status,
                              absl::optional<dbus::ObjectPath> profile_path,
                              absl::optional<std::string> service_path)>;

  // Return callback for the ConfigureESimService method. |service_path|
  // is the path of the newly configured eSIM service. A nullopt |service_path|
  // indicates failure.
  using ConfigureESimServiceCallback =
      base::OnceCallback<void(absl::optional<dbus::ObjectPath> service_path)>;

  // Installs an ESim profile and network with given |activation_code|,
  // |confirmation_code| and |euicc_path|. This method will attempt to create
  // the Shill configuration with given |new_shill_properties| and then enable
  // the newly installed profile and connect to its network afterward.
  // |is_initial_install| is only used for recording eSIM policy install
  // metrics, indicating whether the current attempt is an initial attempt or
  // not.
  void InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      const dbus::ObjectPath& euicc_path,
      base::Value::Dict new_shill_properties,
      InstallProfileFromActivationCodeCallback callback,
      bool is_initial_install = true,
      bool is_install_via_qr_code = false);

  // Attempts to create a Shill service configuration with given
  // |new_shill_properties| for eSIM with |profile_path| and |euicc_path|.
  // |callback| is called with the newly configure service path.
  void ConfigureESimService(const base::Value::Dict& new_shill_properties,
                            const dbus::ObjectPath& euicc_path,
                            const dbus::ObjectPath& profile_path,
                            ConfigureESimServiceCallback callback);

 private:
  friend class CellularESimInstallerTest;
  friend class CellularPolicyHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest,
                           InstallProfileInvalidActivationCode);
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest,
                           InstallProfileConnectFailure);
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest, InstallProfileSuccess);
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest,
                           InstallProfileAutoConnect);
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest,
                           InstallProfileViaQrCodeSuccess);
  FRIEND_TEST_ALL_PREFIXES(CellularESimInstallerTest,
                           InstallProfileAlreadyConnected);
  FRIEND_TEST_ALL_PREFIXES(CellularPolicyHandlerTest, InstallProfileSuccess);
  FRIEND_TEST_ALL_PREFIXES(CellularPolicyHandlerTest, InstallProfileFailure);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class InstallESimProfileResult {
    kSuccess = 0,
    kInhibitFailed = 1,
    kHermesInstallFailed = 2,
    kMaxValue = kHermesInstallFailed
  };

  // Record the result of an attempt to install an eSIM profile either via a
  // QR code or policy configuration. It also records to
  // ESim.Policy.ESimInstall.Initial.OperationResult
  // or ESim.Policy.ESimInstall.Retry.OperationResult histogram to indicate
  // whether the policy eSIM profile installation is an initial attempt or not.
  static void RecordInstallESimProfileResult(InstallESimProfileResult result,
                                             bool is_managed,
                                             bool is_initial_install,
                                             bool is_install_via_qr_code);

  void PerformInstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      const dbus::ObjectPath& euicc_path,
      base::Value::Dict new_shill_properties,
      bool is_initial_install,
      bool is_install_via_qr_code,
      InstallProfileFromActivationCodeCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnProfileInstallResult(
      InstallProfileFromActivationCodeCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      const dbus::ObjectPath& euicc_path,
      const base::Value::Dict& new_shill_properties,
      bool is_initial_install,
      bool is_install_via_qr_code,
      HermesResponseStatus status,
      const dbus::ObjectPath* object_path);
  void OnShillConfigurationCreationSuccess(
      ConfigureESimServiceCallback callback,
      const dbus::ObjectPath& service_path);
  void OnShillConfigurationCreationFailure(
      ConfigureESimServiceCallback callback,
      const std::string& error_name,
      const std::string& error_message);
  void EnableProfile(InstallProfileFromActivationCodeCallback callback,
                     const dbus::ObjectPath& euicc_path,
                     const dbus::ObjectPath& profile_path,
                     absl::optional<dbus::ObjectPath> service_path);
  void OnPrepareCellularNetworkForConnectionSuccess(
      const dbus::ObjectPath& profile_path,
      InstallProfileFromActivationCodeCallback callback,
      const std::string& service_path,
      bool auto_connected);
  void OnPrepareCellularNetworkForConnectionFailure(
      const dbus::ObjectPath& profile_path,
      InstallProfileFromActivationCodeCallback callback,
      const std::string& service_path,
      const std::string& error_name);
  void HandleNewProfileEnableFailure(
      InstallProfileFromActivationCodeCallback callback,
      const dbus::ObjectPath& profile_path,
      const std::string& service_path,
      const std::string& error_name);

  CellularConnectionHandler* cellular_connection_handler_;
  CellularInhibitor* cellular_inhibitor_;

  NetworkConnectionHandler* network_connection_handler_;
  NetworkProfileHandler* network_profile_handler_;
  NetworkStateHandler* network_state_handler_;

  // Maps profile dbus paths to unique pointer of InhibitLocks that are
  // pending to uninhibit.
  std::map<dbus::ObjectPath, std::unique_ptr<CellularInhibitor::InhibitLock>>
      pending_inhibit_locks_;

  base::WeakPtrFactory<CellularESimInstaller> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_ESIM_INSTALLER_H_
