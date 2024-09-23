// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_installer.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/hermes_metrics_util.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using ash::cellular_setup::mojom::ProfileInstallMethod;

namespace ash {
namespace {

void AppendRequiredCellularProperties(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path,
    const NetworkProfile* profile,
    base::Value::Dict* shill_properties_to_update) {
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path);
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  shill_properties_to_update->Set(shill::kTypeProperty, shill::kTypeCellular);
  shill_properties_to_update->Set(shill::kProfileProperty, profile->path);
  // Insert ICCID and EID to the shill properties.
  shill_properties_to_update->Set(shill::kIccidProperty,
                                  profile_properties->iccid().value());
  shill_properties_to_update->Set(shill::kEidProperty,
                                  euicc_properties->eid().value());
  // Use ICCID as GUID if no GUID found in the given properties.
  if (!shill_properties_to_update->FindString(shill::kGuidProperty)) {
    shill_properties_to_update->Set(shill::kGuidProperty,
                                    profile_properties->iccid().value());
  }
}

bool IsManagedNetwork(const base::Value::Dict& new_shill_properties) {
  std::unique_ptr<NetworkUIData> ui_data =
      shill_property_util::GetUIDataFromProperties(new_shill_properties);
  return ui_data && (ui_data->onc_source() == ::onc::ONC_SOURCE_DEVICE_POLICY ||
                     ui_data->onc_source() == ::onc::ONC_SOURCE_USER_POLICY);
}

CellularNetworkMetricsLogger::ESimUserInstallMethod ComputeUserInstallMethod(
    ProfileInstallMethod install_method) {
  switch (install_method) {
    case ProfileInstallMethod::kViaSmds:
      return CellularNetworkMetricsLogger::ESimUserInstallMethod::kViaSmds;
    case ProfileInstallMethod::kViaQrCodeAfterSmds:
      return CellularNetworkMetricsLogger::ESimUserInstallMethod::
          kViaQrCodeAfterSmds;
    case ProfileInstallMethod::kViaQrCodeSkippedSmds:
      return CellularNetworkMetricsLogger::ESimUserInstallMethod::
          kViaQrCodeSkippedSmds;
    case ProfileInstallMethod::kViaActivationCodeAfterSmds:
      return CellularNetworkMetricsLogger::ESimUserInstallMethod::
          kViaActivationCodeAfterSmds;
    case ProfileInstallMethod::kViaActivationCodeSkippedSmds:
      return CellularNetworkMetricsLogger::ESimUserInstallMethod::
          kViaActivationCodeSkippedSmds;
  }
  NOTREACHED();
}

CellularNetworkMetricsLogger::ESimPolicyInstallMethod
ComputePolicyInstallMethod(ProfileInstallMethod install_method) {
  switch (install_method) {
    case ProfileInstallMethod::kViaSmds:
      return CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmds;
    // When installing an eSIM profile via policy we do not have the different
    // methods that are possible via the consumer flow. Default all SM-DP+
    // installation methods to |kViaSmdp|.
    case ProfileInstallMethod::kViaQrCodeAfterSmds:
      [[fallthrough]];
    case ProfileInstallMethod::kViaQrCodeSkippedSmds:
      [[fallthrough]];
    case ProfileInstallMethod::kViaActivationCodeAfterSmds:
      [[fallthrough]];
    case ProfileInstallMethod::kViaActivationCodeSkippedSmds:
      return CellularNetworkMetricsLogger::ESimPolicyInstallMethod::kViaSmdp;
  }
  NOTREACHED();
}

}  // namespace

// static
void CellularESimInstaller::RecordInstallESimProfileResult(
    std::optional<HermesResponseStatus> status,
    bool is_managed,
    bool is_initial_install,
    ProfileInstallMethod install_method) {
  const bool is_user_error =
      status.has_value() &&
      CellularNetworkMetricsLogger::HermesResponseStatusIsUserError(*status);
  const CellularNetworkMetricsLogger::ESimOperationResult result =
      CellularNetworkMetricsLogger::ComputeESimOperationResult(status);

  if (is_managed) {
    CellularNetworkMetricsLogger::LogESimPolicyInstallResult(
        ComputePolicyInstallMethod(install_method), result, is_initial_install,
        is_user_error);
    return;
  }
  CellularNetworkMetricsLogger::LogESimUserInstallResult(
      ComputeUserInstallMethod(install_method), result, is_user_error);
}

CellularESimInstaller::CellularESimInstaller() = default;

CellularESimInstaller::~CellularESimInstaller() = default;

void CellularESimInstaller::Init(
    CellularConnectionHandler* cellular_connection_handler,
    CellularInhibitor* cellular_inhibitor,
    NetworkConnectionHandler* network_connection_handler,
    NetworkProfileHandler* network_profile_handler,
    NetworkStateHandler* network_state_handler) {
  cellular_connection_handler_ = cellular_connection_handler;
  cellular_inhibitor_ = cellular_inhibitor;
  network_connection_handler_ = network_connection_handler;
  network_profile_handler_ = network_profile_handler;
  network_state_handler_ = network_state_handler;
}

void CellularESimInstaller::InstallProfileFromActivationCode(
    const std::string& activation_code,
    const std::string& confirmation_code,
    const dbus::ObjectPath& euicc_path,
    base::Value::Dict new_shill_properties,
    InstallProfileFromActivationCodeCallback callback,
    bool is_initial_install,
    ProfileInstallMethod install_method) {
  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kInstallingProfile,
      base::BindOnce(
          &CellularESimInstaller::PerformInstallProfileFromActivationCode,
          weak_ptr_factory_.GetWeakPtr(), activation_code, confirmation_code,
          euicc_path, std::move(new_shill_properties), is_initial_install,
          install_method, std::move(callback)));
}

void CellularESimInstaller::PerformInstallProfileFromActivationCode(
    const std::string& activation_code,
    const std::string& confirmation_code,
    const dbus::ObjectPath& euicc_path,
    base::Value::Dict new_shill_properties,
    bool is_initial_install,
    ProfileInstallMethod install_method,
    InstallProfileFromActivationCodeCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhibiting cellular device";

    RecordInstallESimProfileResult(
        /*status=*/std::nullopt, IsManagedNetwork(new_shill_properties),
        is_initial_install, install_method);

    std::move(callback).Run(HermesResponseStatus::kErrorWrongState,
                            /*profile_path=*/std::nullopt,
                            /*service_path=*/std::nullopt);
    return;
  }

  // TODO(crbug.com/1186682) Add a check for activation codes that are currently
  // being installed to prevent multiple attempts for the same activation code.
  NET_LOG(USER) << "Attempting installation with code " << activation_code;

  HermesEuiccClient::Get()->InstallProfileFromActivationCode(
      euicc_path, activation_code, confirmation_code,
      base::BindOnce(&CellularESimInstaller::OnProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(inhibit_lock), euicc_path,
                     std::move(new_shill_properties), is_initial_install,
                     install_method, base::Time::Now()));
}

void CellularESimInstaller::OnProfileInstallResult(
    InstallProfileFromActivationCodeCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    const dbus::ObjectPath& euicc_path,
    const base::Value::Dict& new_shill_properties,
    bool is_initial_install,
    ProfileInstallMethod install_method,
    const base::Time installation_start_time,
    HermesResponseStatus status,
    dbus::DBusResult dbusResult,
    const dbus::ObjectPath* profile_path) {
  hermes_metrics::LogInstallViaQrCodeResult(status, dbusResult,
                                            is_initial_install);

  RecordInstallESimProfileResult(status, IsManagedNetwork(new_shill_properties),
                                 is_initial_install, install_method);

  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing profile status=" << status;
    std::move(callback).Run(status, /*profile_path=*/std::nullopt,
                            /*service_path=*/std::nullopt);
    return;
  }

  UMA_HISTOGRAM_LONG_TIMES_100(
      "Network.Cellular.ESim.ProfileDownload.ActivationCode.Latency",
      base::Time::Now() - installation_start_time);

  pending_inhibit_locks_.emplace(*profile_path, std::move(inhibit_lock));
  ConfigureESimService(
      new_shill_properties, euicc_path, *profile_path,
      base::BindOnce(&CellularESimInstaller::EnableProfile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     euicc_path, *profile_path));
}

void CellularESimInstaller::ConfigureESimService(
    const base::Value::Dict& new_shill_properties,
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path,
    ConfigureESimServiceCallback callback) {
  const NetworkProfile* profile =
      cellular_utils::GetCellularProfile(network_profile_handler_);

  if (!profile) {
    NET_LOG(ERROR)
        << "Error configuring eSIM profile. Default profile not initialized.";
    std::move(callback).Run(
        /*service_path=*/std::nullopt);
    return;
  }

  base::Value::Dict properties_to_set = new_shill_properties.Clone();
  AppendRequiredCellularProperties(euicc_path, profile_path, profile,
                                   &properties_to_set);
  NET_LOG(EVENT)
      << "Creating shill configuration for eSim profile with properties: "
      << properties_to_set;

  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->ConfigureServiceForProfile(
      dbus::ObjectPath(profile->path), properties_to_set,
      base::BindOnce(
          &CellularESimInstaller::OnShillConfigurationCreationSuccess,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback_split.first)),
      base::BindOnce(
          &CellularESimInstaller::OnShillConfigurationCreationFailure,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback_split.second)));
}

void CellularESimInstaller::OnShillConfigurationCreationSuccess(
    ConfigureESimServiceCallback callback,
    const dbus::ObjectPath& service_path) {
  NET_LOG(EVENT)
      << "Successfully creating shill configuration on service path: "
      << service_path.value();
  std::move(callback).Run(service_path);
}

void CellularESimInstaller::OnShillConfigurationCreationFailure(
    ConfigureESimServiceCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Create shill configuration failed, error:" << error_name
                 << ", message: " << error_message;
  std::move(callback).Run(/*service_path=*/std::nullopt);
}

void CellularESimInstaller::EnableProfile(
    InstallProfileFromActivationCodeCallback callback,
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path,
    std::optional<dbus::ObjectPath> service_path) {
  auto it = pending_inhibit_locks_.find(profile_path);
  DCHECK(it != pending_inhibit_locks_.end());

  auto callback_split = base::SplitOnceCallback(std::move(callback));
  cellular_connection_handler_
      ->PrepareNewlyInstalledCellularNetworkForConnection(
          euicc_path, profile_path, std::move(it->second),
          base::BindOnce(&CellularESimInstaller::
                             OnPrepareCellularNetworkForConnectionSuccess,
                         weak_ptr_factory_.GetWeakPtr(), profile_path,
                         std::move(callback_split.first)),
          base::BindOnce(&CellularESimInstaller::
                             OnPrepareCellularNetworkForConnectionFailure,
                         weak_ptr_factory_.GetWeakPtr(), profile_path,
                         std::move(callback_split.second)));
  pending_inhibit_locks_.erase(it);
}

void CellularESimInstaller::OnPrepareCellularNetworkForConnectionSuccess(
    const dbus::ObjectPath& profile_path,
    InstallProfileFromActivationCodeCallback callback,
    const std::string& service_path,
    bool auto_connected) {
  NET_LOG(EVENT) << "Successfully enabled installed profile on service path: "
                 << service_path;
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state) {
    HandleNewProfileEnableFailure(std::move(callback), profile_path,
                                  service_path,
                                  NetworkConnectionHandler::kErrorNotFound);
    return;
  }

  if (!network_state->IsConnectingOrConnected()) {
    // The connection could fail but the user will be notified of connection
    // failures separately.
    network_connection_handler_->ConnectToNetwork(
        service_path,
        /*success_callback=*/base::DoNothing(),
        /*error_callback=*/base::DoNothing(),
        /*check_error_state=*/false, ConnectCallbackMode::ON_STARTED);
  }

  std::move(callback).Run(HermesResponseStatus::kSuccess, profile_path,
                          service_path);
}

void CellularESimInstaller::OnPrepareCellularNetworkForConnectionFailure(
    const dbus::ObjectPath& profile_path,
    InstallProfileFromActivationCodeCallback callback,
    const std::string& service_path,
    const std::string& error_name) {
  HandleNewProfileEnableFailure(std::move(callback), profile_path, service_path,
                                error_name);
}

void CellularESimInstaller::HandleNewProfileEnableFailure(
    InstallProfileFromActivationCodeCallback callback,
    const dbus::ObjectPath& profile_path,
    const std::string& service_path,
    const std::string& error_name) {
  NET_LOG(ERROR) << "Error enabling newly created profile path="
                 << profile_path.value() << ", service path=" << service_path
                 << ", error_name=" << error_name;
  // Propagate |profile_path| and |service_path| so that the code that
  // initiated the installation can handle the case where the profile was
  // successfully installed, but the installation process failed for some
  // other reason e.g. failed to enable the profile.
  std::move(callback).Run(HermesResponseStatus::kErrorWrongState,
                          /*profile_path=*/profile_path,
                          /*service_path=*/service_path);
}

}  // namespace ash
