// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/esim_profile.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_esim_profile.h"
#include "chromeos/ash/components/network/cellular_esim_uninstall_handler.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/hermes_metrics_util.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/cellular_setup/esim_manager.h"
#include "chromeos/ash/services/cellular_setup/esim_mojo_utils.h"
#include "chromeos/ash/services/cellular_setup/euicc.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-shared.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_manager.h"
#include "dbus/object_path.h"

namespace ash::cellular_setup {

namespace {

bool IsGuestModeActive() {
  return user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
         user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession();
}

bool IsESimProfilePropertiesEqualToState(
    const mojom::ESimProfilePropertiesPtr& properties,
    const CellularESimProfile& esim_profile_state) {
  return esim_profile_state.iccid() == properties->iccid &&
         esim_profile_state.name() == properties->name &&
         esim_profile_state.nickname() == properties->nickname &&
         esim_profile_state.service_provider() ==
             properties->service_provider &&
         ProfileStateToMojo(esim_profile_state.state()) == properties->state &&
         esim_profile_state.activation_code() == properties->activation_code;
}

// Measures the time from which this function is called to when |callback|
// is expected to run. The measured time difference should capture the time it
// took for a pending profile to be fully downloaded.
ESimProfile::InstallProfileCallback CreateTimedInstallProfileCallback(
    ESimProfile::InstallProfileCallback callback) {
  return base::BindOnce(
      [](ESimProfile::InstallProfileCallback callback,
         base::Time installation_start_time,
         mojom::ProfileInstallResult result) -> void {
        std::move(callback).Run(result);
        if (result != mojom::ProfileInstallResult::kSuccess)
          return;
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Network.Cellular.ESim.ProfileDownload.PendingProfile.Latency",
            base::Time::Now() - installation_start_time);
      },
      std::move(callback), base::Time::Now());
}

}  // namespace

ESimProfile::ESimProfile(const CellularESimProfile& esim_profile_state,
                         Euicc* euicc,
                         ESimManager* esim_manager)
    : euicc_(euicc),
      esim_manager_(esim_manager),
      properties_(mojom::ESimProfileProperties::New()),
      path_(esim_profile_state.path()) {
  UpdateProperties(esim_profile_state, /*notify=*/false);
  properties_->eid = euicc->properties()->eid;
}

ESimProfile::~ESimProfile() {
  if (install_callback_) {
    NET_LOG(ERROR) << "Profile destroyed with unfulfilled install callback";
  }
  if (uninstall_callback_) {
    NET_LOG(ERROR) << "Profile destroyed with unfulfilled uninstall callbacks";
  }
  if (set_profile_nickname_callback_) {
    NET_LOG(ERROR)
        << "Profile destroyed with unfulfilled set profile nickname callbacks";
  }
}

void ESimProfile::GetProperties(GetPropertiesCallback callback) {
  std::move(callback).Run(properties_->Clone());
}

void ESimProfile::InstallProfile(const std::string& confirmation_code,
                                 InstallProfileCallback callback) {
  if (properties_->state == mojom::ProfileState::kInstalling ||
      properties_->state != mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Profile is already installed or in installing state.";
    std::move(callback).Run(mojom::ProfileInstallResult::kFailure);
    return;
  }

  properties_->state = mojom::ProfileState::kInstalling;
  esim_manager_->NotifyESimProfileChanged(this);

  NET_LOG(USER) << "Installing profile with path " << path().value();
  install_callback_ = CreateTimedInstallProfileCallback(std::move(callback));
  EnsureProfileExistsOnEuiccCallback perform_install_profile_callback =
      base::BindOnce(&ESimProfile::PerformInstallProfile,
                     weak_ptr_factory_.GetWeakPtr(), confirmation_code);
  esim_manager_->cellular_inhibitor()->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kInstallingProfile,
      base::BindOnce(&ESimProfile::EnsureProfileExistsOnEuicc,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(perform_install_profile_callback)));
}

void ESimProfile::UninstallProfile(UninstallProfileCallback callback) {
  if (IsGuestModeActive()) {
    NET_LOG(ERROR) << "Cannot uninstall profile in guest mode.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  if (!IsProfileInstalled()) {
    NET_LOG(ERROR) << "Profile uninstall failed: Profile is not installed.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  if (IsProfileManaged()) {
    NET_LOG(ERROR)
        << "Profile uninstall failed: Cannot uninstall managed profile.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  NET_LOG(USER) << "Uninstalling profile with path " << path().value();
  uninstall_callback_ = base::BindOnce(
      [](UninstallProfileCallback callback,
         mojom::ESimOperationResult result) -> void {
        base::UmaHistogramBoolean(
            "Network.Cellular.ESim.ProfileUninstallationResult",
            result == mojom::ESimOperationResult::kSuccess);
        std::move(callback).Run(result);
      },
      std::move(callback));

  esim_manager_->cellular_esim_uninstall_handler()->UninstallESim(
      properties_->iccid, path_, euicc_->path(),
      base::BindOnce(&ESimProfile::OnProfileUninstallResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ESimProfile::SetProfileNickname(const std::u16string& nickname,
                                     SetProfileNicknameCallback callback) {
  if (IsGuestModeActive()) {
    NET_LOG(ERROR) << "Cannot rename profile in guest mode.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  if (IsProfileManaged()) {
    NET_LOG(ERROR) << "Cannot rename managed profile.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  if (set_profile_nickname_callback_) {
    NET_LOG(ERROR) << "Set Profile Nickname already in progress.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  if (properties_->state == mojom::ProfileState::kInstalling ||
      properties_->state == mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Set Profile Nickname failed: Profile is not installed.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  NET_LOG(USER) << "Setting profile nickname for path " << path().value();
  set_profile_nickname_callback_ = base::BindOnce(
      [](SetProfileNicknameCallback callback,
         mojom::ESimOperationResult result) -> void {
        base::UmaHistogramBoolean(
            "Network.Cellular.ESim.ProfileRenameResult",
            result == mojom::ESimOperationResult::kSuccess);
        std::move(callback).Run(result);
      },
      std::move(callback));

  EnsureProfileExistsOnEuiccCallback perform_set_profile_nickname_callback =
      base::BindOnce(&ESimProfile::PerformSetProfileNickname,
                     weak_ptr_factory_.GetWeakPtr(), nickname);
  esim_manager_->cellular_inhibitor()->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kRenamingProfile,
      base::BindOnce(&ESimProfile::EnsureProfileExistsOnEuicc,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(perform_set_profile_nickname_callback)));
}

void ESimProfile::UpdateProperties(
    const CellularESimProfile& esim_profile_state,
    bool notify) {
  if (IsESimProfilePropertiesEqualToState(properties_, esim_profile_state)) {
    return;
  }

  properties_->iccid = esim_profile_state.iccid();
  properties_->name = esim_profile_state.name();
  properties_->nickname = esim_profile_state.nickname();
  properties_->service_provider = esim_profile_state.service_provider();
  properties_->state = ProfileStateToMojo(esim_profile_state.state());
  properties_->activation_code = esim_profile_state.activation_code();
  if (notify) {
    esim_manager_->NotifyESimProfileChanged(this);
    esim_manager_->NotifyESimProfileListChanged(euicc_);
  }
}

void ESimProfile::OnProfileRemove() {
  // Run pending callbacks before profile is removed.
  if (uninstall_callback_) {
    // This profile could be removed before UninstallHandler returns. Return a
    // success since the profile will be removed.
    std::move(uninstall_callback_).Run(mojom::ESimOperationResult::kSuccess);
  }

  // Installation or setting nickname could trigger a request for profiles. If
  // this profile gets removed at that point, return the pending call with
  // failure.
  if (install_callback_) {
    std::move(install_callback_).Run(mojom::ProfileInstallResult::kFailure);
  }
  if (set_profile_nickname_callback_) {
    std::move(set_profile_nickname_callback_)
        .Run(mojom::ESimOperationResult::kFailure);
  }
}

mojo::PendingRemote<mojom::ESimProfile> ESimProfile::CreateRemote() {
  mojo::PendingRemote<mojom::ESimProfile> esim_profile_remote;
  receiver_set_.Add(this, esim_profile_remote.InitWithNewPipeAndPassReceiver());
  return esim_profile_remote;
}

void ESimProfile::EnsureProfileExistsOnEuicc(
    EnsureProfileExistsOnEuiccCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhibiting cellular device";
    std::move(callback).Run(/*request_profile_success=*/false,
                            /*inhibit_lock=*/nullptr);
    return;
  }

  if (!ProfileExistsOnEuicc()) {
    if (IsProfileInstalled()) {
      esim_manager_->cellular_esim_profile_handler()->RefreshProfileList(
          euicc_->path(),
          base::BindOnce(&ESimProfile::OnRequestInstalledProfiles,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          std::move(inhibit_lock));
    } else {
      HermesEuiccClient::Get()->RefreshSmdxProfiles(
          euicc_->path(),
          /*activation_code=*/ESimManager::GetRootSmdsAddress(),
          /*restore_slot=*/true,
          base::BindOnce(&ESimProfile::OnRefreshSmdxProfiles,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(inhibit_lock)));
    }
    return;
  }

  std::move(callback).Run(/*request_profile_success=*/true,
                          std::move(inhibit_lock));
}

void ESimProfile::OnRequestInstalledProfiles(
    EnsureProfileExistsOnEuiccCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  bool success = inhibit_lock != nullptr;
  if (!success) {
    NET_LOG(ERROR) << "Error requesting installed profiles to ensure profile "
                   << "exists on Euicc";
  }
  OnRequestProfiles(std::move(callback), std::move(inhibit_lock), success);
}

void ESimProfile::OnRefreshSmdxProfiles(
    EnsureProfileExistsOnEuiccCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status,
    const std::vector<dbus::ObjectPath>& profile_paths) {
  NET_LOG(EVENT) << "Refresh SM-DX profiles found " << profile_paths.size()
                 << " available profiles";
  bool success = status == HermesResponseStatus::kSuccess;
  if (!success) {
    NET_LOG(ERROR) << "Error refreshing SM-DX profiles to ensure profile "
                   << "exists on Euicc; status: " << status;
  }
  OnRequestProfiles(std::move(callback), std::move(inhibit_lock), success);
}

void ESimProfile::OnRequestPendingProfiles(
    EnsureProfileExistsOnEuiccCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status) {
  bool success = status == HermesResponseStatus::kSuccess;
  if (!success) {
    NET_LOG(ERROR) << "Error requesting pending profiles to ensure profile "
                   << "exists on Euicc; status: " << status;
  }
  OnRequestProfiles(std::move(callback), std::move(inhibit_lock), success);
}

void ESimProfile::OnRequestProfiles(
    EnsureProfileExistsOnEuiccCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    bool success) {
  if (!success) {
    std::move(callback).Run(/*request_profile_success=*/false,
                            std::move(inhibit_lock));
    return;
  }

  // If profile does not exist on Euicc even after request for profiles then
  // return failure. The profile was removed and this object will get destroyed
  // when CellularESimProfileHandler updates.
  if (!ProfileExistsOnEuicc()) {
    NET_LOG(ERROR) << "Unable to ensure profile exists on Euicc. path="
                   << path_.value();
    std::move(callback).Run(/*request_profile_success=*/false,
                            std::move(inhibit_lock));
    return;
  }

  std::move(callback).Run(/*request_profile_success=*/true,
                          std::move(inhibit_lock));
}

void ESimProfile::PerformInstallProfile(
    const std::string& confirmation_code,
    bool request_profile_success,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!request_profile_success) {
    properties_->state = mojom::ProfileState::kPending;
    esim_manager_->NotifyESimProfileChanged(this);
    std::move(install_callback_).Run(mojom::ProfileInstallResult::kFailure);
    return;
  }

  HermesEuiccClient::Get()->InstallPendingProfile(
      euicc_->path(), path_, confirmation_code,
      base::BindOnce(&ESimProfile::OnPendingProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(inhibit_lock)));
}

void ESimProfile::PerformSetProfileNickname(
    const std::u16string& nickname,
    bool request_profile_success,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!request_profile_success) {
    std::move(set_profile_nickname_callback_)
        .Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  HermesProfileClient::Get()->RenameProfile(
      path_, base::UTF16ToUTF8(nickname),
      base::BindOnce(&ESimProfile::OnProfileNicknameSet,
                     weak_ptr_factory_.GetWeakPtr(), std::move(inhibit_lock)));
}

void ESimProfile::OnPendingProfileInstallResult(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status) {
  hermes_metrics::LogInstallPendingProfileResult(status);

  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing pending profile status=" << status;
    properties_->state = mojom::ProfileState::kPending;
    esim_manager_->NotifyESimProfileChanged(this);
    std::move(install_callback_).Run(InstallResultFromStatus(status));
    return;
  }

  // inhibit_lock will be released by esim connection handler.
  // Cellular device will uninhibit automatically at that point.
  esim_manager_->cellular_connection_handler()
      ->PrepareNewlyInstalledCellularNetworkForConnection(
          euicc_->path(), path_, std::move(inhibit_lock),
          base::BindOnce(&ESimProfile::OnNewProfileEnableSuccess,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(
              &ESimProfile::OnPrepareCellularNetworkForConnectionFailure,
              weak_ptr_factory_.GetWeakPtr()));
}

void ESimProfile::OnNewProfileEnableSuccess(const std::string& service_path,
                                            bool auto_connected) {
  const NetworkState* network_state =
      esim_manager_->network_state_handler()->GetNetworkState(service_path);
  if (!network_state) {
    OnPrepareCellularNetworkForConnectionFailure(
        service_path, NetworkConnectionHandler::kErrorNotFound);
    return;
  }

  if (!network_state->IsConnectingOrConnected()) {
    // The connection could fail but the user will be notified of connection
    // failures separately.
    esim_manager_->network_connection_handler()->ConnectToNetwork(
        service_path, /*success_callback=*/base::DoNothing(),
        /*error_callback=*/base::DoNothing(),
        /*check_error_state=*/false, ConnectCallbackMode::ON_STARTED);
  }

  DCHECK(install_callback_);
  std::move(install_callback_).Run(mojom::ProfileInstallResult::kSuccess);
}

void ESimProfile::OnPrepareCellularNetworkForConnectionFailure(
    const std::string& service_path,
    const std::string& error_name) {
  NET_LOG(ERROR) << "Error preparing network for connection. "
                 << "Error: " << error_name
                 << ", Service path: " << service_path;
  std::move(install_callback_).Run(mojom::ProfileInstallResult::kFailure);
}

void ESimProfile::OnProfileUninstallResult(bool success) {
  std::move(uninstall_callback_)
      .Run(success ? mojom::ESimOperationResult::kSuccess
                   : mojom::ESimOperationResult::kFailure);
}

void ESimProfile::OnProfileNicknameSet(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "ESimProfile rename error.";
  }
  std::move(set_profile_nickname_callback_)
      .Run(status == HermesResponseStatus::kSuccess
               ? mojom::ESimOperationResult::kSuccess
               : mojom::ESimOperationResult::kFailure);
  // inhibit_lock goes out of scope and will uninhibit automatically.
}

bool ESimProfile::ProfileExistsOnEuicc() {
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_->path());

  return base::Contains(euicc_properties->profiles().value(), path_);
}

bool ESimProfile::IsProfileInstalled() {
  return properties_->state != mojom::ProfileState::kPending &&
         properties_->state != mojom::ProfileState::kInstalling;
}

bool ESimProfile::IsProfileManaged() {
  NetworkStateHandler::NetworkStateList networks;
  esim_manager_->network_state_handler()->GetNetworkListByType(
      NetworkTypePattern::Cellular(),
      /*configure_only=*/false, /*visible=*/false, /*limit=*/0, &networks);
  for (const NetworkState* network : networks) {
    if (network->iccid() == properties_->iccid)
      return network->IsManagedByPolicy();
  }
  return false;
}

}  // namespace ash::cellular_setup
