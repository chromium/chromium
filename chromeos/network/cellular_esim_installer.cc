// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_installer.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/network/cellular_connection_handler.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/hermes_metrics_util.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {
namespace {

// Measures the time from which this function is called to when |callback|
// is expected to run. The measured time difference should capture the time it
// took for a profile to be fully downloaded from a provided activation code.
CellularESimInstaller::InstallProfileFromActivationCodeCallback
CreateTimedInstallProfileCallback(
    CellularESimInstaller::InstallProfileFromActivationCodeCallback callback) {
  return base::BindOnce(
      [](CellularESimInstaller::InstallProfileFromActivationCodeCallback
             callback,
         base::Time installation_start_time, HermesResponseStatus result,
         absl::optional<dbus::ObjectPath> esim_profile_path) -> void {
        std::move(callback).Run(result, esim_profile_path);
        if (result != HermesResponseStatus::kSuccess)
          return;
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Network.Cellular.ESim.ProfileDownload.ActivationCode.Latency",
            base::Time::Now() - installation_start_time);
      },
      std::move(callback), base::Time::Now());
}
}  // namespace

// static
void CellularESimInstaller::RecordInstallProfileViaQrCodeResult(
    InstallProfileViaQrCodeResult result) {
  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.InstallViaQrCode.OperationResult", result);
}

CellularESimInstaller::CellularESimInstaller() = default;

CellularESimInstaller::~CellularESimInstaller() = default;

void CellularESimInstaller::Init(
    CellularConnectionHandler* cellular_connection_handler,
    CellularInhibitor* cellular_inhibitor,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler) {
  cellular_connection_handler_ = cellular_connection_handler;
  cellular_inhibitor_ = cellular_inhibitor;
  network_connection_handler_ = network_connection_handler;
  network_state_handler_ = network_state_handler;
}

void CellularESimInstaller::InstallProfileFromActivationCode(
    const std::string& activation_code,
    const std::string& confirmation_code,
    const dbus::ObjectPath& euicc_path,
    InstallProfileFromActivationCodeCallback callback) {
  // Try installing directly with activation code.
  // TODO(crbug.com/1186682) Add a check for activation codes that are
  // currently being installed to prevent multiple attempts for the same
  // activation code.
  NET_LOG(USER) << "Attempting installation with code " << activation_code;
  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kInstallingProfile,
      base::BindOnce(
          &CellularESimInstaller::PerformInstallProfileFromActivationCode,
          weak_ptr_factory_.GetWeakPtr(), activation_code, confirmation_code,
          euicc_path, CreateTimedInstallProfileCallback(std::move(callback))));
}

void CellularESimInstaller::PerformInstallProfileFromActivationCode(
    const std::string& activation_code,
    const std::string& confirmation_code,
    const dbus::ObjectPath& euicc_path,
    InstallProfileFromActivationCodeCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhibiting cellular device";
    RecordInstallProfileViaQrCodeResult(
        InstallProfileViaQrCodeResult::kInhibitFailed);
    std::move(callback).Run(HermesResponseStatus::kErrorWrongState,
                            absl::nullopt);
    return;
  }

  HermesEuiccClient::Get()->InstallProfileFromActivationCode(
      euicc_path, activation_code, confirmation_code,
      base::BindOnce(&CellularESimInstaller::OnProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(inhibit_lock), euicc_path));
}

void CellularESimInstaller::OnProfileInstallResult(
    InstallProfileFromActivationCodeCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    const dbus::ObjectPath& euicc_path,
    HermesResponseStatus status,
    const dbus::ObjectPath* profile_path) {
  hermes_metrics::LogInstallViaQrCodeResult(status);

  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing profile status="
                   << static_cast<int>(status);
    RecordInstallProfileViaQrCodeResult(
        InstallProfileViaQrCodeResult::kHermesInstallFailed);
    std::move(callback).Run(status, absl::nullopt);
    return;
  }

  RecordInstallProfileViaQrCodeResult(InstallProfileViaQrCodeResult::kSuccess);

  install_calls_pending_connect_.emplace(*profile_path, std::move(callback));
  cellular_connection_handler_
      ->PrepareNewlyInstalledCellularNetworkForConnection(
          euicc_path, *profile_path, std::move(inhibit_lock),
          base::BindOnce(&CellularESimInstaller::
                             OnPrepareCellularNetworkForConnectionSuccess,
                         weak_ptr_factory_.GetWeakPtr(), *profile_path),
          base::BindOnce(&CellularESimInstaller::
                             OnPrepareCellularNetworkForConnectionFailure,
                         weak_ptr_factory_.GetWeakPtr(), *profile_path));
}

void CellularESimInstaller::OnPrepareCellularNetworkForConnectionSuccess(
    const dbus::ObjectPath& profile_path,
    const std::string& service_path) {
  const NetworkState* network_state =
      network_state_handler_->GetNetworkState(service_path);
  if (!network_state) {
    HandleNewProfileEnableFailure(profile_path,
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

  auto it = install_calls_pending_connect_.find(profile_path);
  DCHECK(it != install_calls_pending_connect_.end());

  InstallProfileFromActivationCodeCallback callback = std::move(it->second);
  install_calls_pending_connect_.erase(it);

  std::move(callback).Run(HermesResponseStatus::kSuccess, profile_path);
}

void CellularESimInstaller::OnPrepareCellularNetworkForConnectionFailure(
    const dbus::ObjectPath& profile_path,
    const std::string& service_path,
    const std::string& error_name) {
  HandleNewProfileEnableFailure(profile_path, error_name);
}

void CellularESimInstaller::HandleNewProfileEnableFailure(
    const dbus::ObjectPath& profile_path,
    const std::string& error_name) {
  NET_LOG(ERROR) << "Error enabling newly created profile path="
                 << profile_path.value() << " error_name=" << error_name;

  auto it = install_calls_pending_connect_.find(profile_path);
  DCHECK(it != install_calls_pending_connect_.end());
  std::move(it->second)
      .Run(HermesResponseStatus::kErrorWrongState, absl::nullopt);
  install_calls_pending_connect_.erase(it);
}

}  // namespace chromeos