// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/euicc.h"

#include <cstdint>
#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chromeos/network/cellular_connection_handler.h"
#include "chromeos/network/cellular_esim_profile.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/cellular_setup/esim_manager.h"
#include "chromeos/services/cellular_setup/esim_mojo_utils.h"
#include "chromeos/services/cellular_setup/esim_profile.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-shared.h"
#include "components/device_event_log/device_event_log.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "dbus/object_path.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {
namespace cellular_setup {
namespace {

// Prefix for EID when encoded in QR Code.
const char kEidQrCodePrefix[] = "EID:";

// Measures the time from which this function is called to when |callback|
// is expected to run. The measured time difference should capture the time it
// took for a profile to be fully downloaded from a provided activation code.
Euicc::InstallProfileFromActivationCodeCallback
CreateTimedInstallProfileCallback(
    Euicc::InstallProfileFromActivationCodeCallback callback) {
  return base::BindOnce(
      [](Euicc::InstallProfileFromActivationCodeCallback callback,
         base::Time installation_start_time, mojom::ProfileInstallResult result,
         mojo::PendingRemote<mojom::ESimProfile> esim_profile_pending_remote)
          -> void {
        std::move(callback).Run(result, std::move(esim_profile_pending_remote));
        if (result != mojom::ProfileInstallResult::kSuccess)
          return;
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Network.Cellular.ESim.ProfileDownload.ActivationCode.Latency",
            base::Time::Now() - installation_start_time);
      },
      std::move(callback), base::Time::Now());
}

// Measures the time from which this function is called to when |callback|
// is expected to run. The measured time difference should capture the time it
// took for a profile discovery request to complete.
Euicc::RequestPendingProfilesCallback CreateTimedRequestPendingProfilesCallback(
    Euicc::RequestPendingProfilesCallback callback) {
  return base::BindOnce(
      [](Euicc::RequestPendingProfilesCallback callback,
         base::Time installation_start_time,
         mojom::ESimOperationResult result) -> void {
        std::move(callback).Run(result);
        if (result != mojom::ESimOperationResult::kSuccess)
          return;
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Network.Cellular.ESim.ProfileDiscovery.Latency",
            base::Time::Now() - installation_start_time);
      },
      std::move(callback), base::Time::Now());
}
}  // namespace

Euicc::Euicc(const dbus::ObjectPath& path, ESimManager* esim_manager)
    : esim_manager_(esim_manager),
      properties_(mojom::EuiccProperties::New()),
      path_(path) {
  UpdateProperties();
}

Euicc::~Euicc() = default;

void Euicc::GetProperties(GetPropertiesCallback callback) {
  std::move(callback).Run(properties_->Clone());
}

void Euicc::GetProfileList(GetProfileListCallback callback) {
  std::vector<mojo::PendingRemote<mojom::ESimProfile>> remote_list;
  for (auto& esim_profile : esim_profiles_) {
    remote_list.push_back(esim_profile->CreateRemote());
  }
  std::move(callback).Run(std::move(remote_list));
}

void Euicc::InstallProfileFromActivationCode(
    const std::string& activation_code,
    const std::string& confirmation_code,
    InstallProfileFromActivationCodeCallback callback) {
  ESimProfile* profile_info = nullptr;
  mojom::ProfileInstallResult status =
      GetPendingProfileInfoFromActivationCode(activation_code, &profile_info);

  // Return early if profile was found but not in the correct state.
  if (profile_info && status != mojom::ProfileInstallResult::kSuccess) {
    NET_LOG(ERROR) << "EUICC could not install profile: " << status;
    std::move(callback).Run(status, mojo::NullRemote());
    return;
  }

  if (profile_info) {
    NET_LOG(USER) << "Installing profile with path "
                  << profile_info->path().value();
    profile_info->InstallProfile(
        confirmation_code,
        base::BindOnce(
            [](InstallProfileFromActivationCodeCallback callback,
               ESimProfile* esim_profile,
               mojom::ProfileInstallResult status) -> void {
              std::move(callback).Run(status, esim_profile->CreateRemote());
            },
            std::move(callback), profile_info));
    return;
  }

  // Try installing directly with activation code.
  // TODO(crbug.com/1186682) Add a check for activation codes that are
  // currently being installed to prevent multiple attempts for the same
  // activation code.
  NET_LOG(USER) << "Attempting installation with code " << activation_code;
  esim_manager_->cellular_inhibitor()->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kInstallingProfile,
      base::BindOnce(&Euicc::PerformInstallProfileFromActivationCode,
                     weak_ptr_factory_.GetWeakPtr(), activation_code,
                     confirmation_code,
                     CreateTimedInstallProfileCallback(std::move(callback))));
}

void Euicc::RequestPendingProfiles(RequestPendingProfilesCallback callback) {
  NET_LOG(EVENT) << "Requesting Pending profiles";
  esim_manager_->cellular_inhibitor()->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kRefreshingProfileList,
      base::BindOnce(
          &Euicc::PerformRequestPendingProfiles, weak_ptr_factory_.GetWeakPtr(),
          CreateTimedRequestPendingProfilesCallback(std::move(callback))));
}

void Euicc::GetEidQRCode(GetEidQRCodeCallback callback) {
  // Format EID to string that should be encoded in the QRCode.
  std::string qr_code_string =
      base::StrCat({kEidQrCodePrefix, properties_->eid});
  QRCodeGenerator qr_generator;
  base::Optional<QRCodeGenerator::GeneratedCode> qr_data =
      qr_generator.Generate(base::as_bytes(
          base::make_span(qr_code_string.data(), qr_code_string.size())));
  if (!qr_data || qr_data->data.data() == nullptr ||
      qr_data->data.size() == 0) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Data returned from QRCodeGenerator consist of bytes that represents
  // tiles. Least significant bit of each byte is set if the tile should be
  // filled. Other bit positions indicate QR Code structure and are not required
  // for rendering. Convert this data to 0 or 1 values for simpler UI side
  // rendering.
  for (uint8_t& qr_data_byte : qr_data->data) {
    qr_data_byte &= 1;
  }

  mojom::QRCodePtr qr_code = mojom::QRCode::New();
  qr_code->size = qr_data->qr_size;
  qr_code->data.assign(qr_data->data.begin(), qr_data->data.end());
  std::move(callback).Run(std::move(qr_code));
}

void Euicc::UpdateProfileList(
    const std::vector<CellularESimProfile>& esim_profile_states) {
  std::vector<ESimProfile*> newly_created_profiles;
  bool profile_list_changed = false;
  for (auto esim_profile_state : esim_profile_states) {
    if (esim_profile_state.eid() != properties_->eid) {
      continue;
    }
    ESimProfile* new_profile = UpdateOrCreateESimProfile(esim_profile_state);
    if (new_profile) {
      profile_list_changed = true;
      newly_created_profiles.push_back(new_profile);
    }
  }
  profile_list_changed |= RemoveUntrackedProfiles(esim_profile_states);
  if (profile_list_changed) {
    esim_manager_->NotifyESimProfileListChanged(this);

    // Run any install callbacks that are pending creation of new ESimProfile
    // object.
    for (ESimProfile* esim_profile : newly_created_profiles) {
      auto it = install_calls_pending_create_.find(esim_profile->path());
      if (it == install_calls_pending_create_.end()) {
        continue;
      }
      std::move(it->second)
          .Run(mojom::ProfileInstallResult::kSuccess,
               esim_profile->CreateRemote());
      install_calls_pending_create_.erase(it);
    }
  }
}

void Euicc::UpdateProperties() {
  HermesEuiccClient::Properties* properties =
      HermesEuiccClient::Get()->GetProperties(path_);
  properties_->eid = properties->eid().value();
  properties_->is_active = properties->is_active().value();
}

mojo::PendingRemote<mojom::Euicc> Euicc::CreateRemote() {
  mojo::PendingRemote<mojom::Euicc> euicc_remote;
  receiver_set_.Add(this, euicc_remote.InitWithNewPipeAndPassReceiver());
  return euicc_remote;
}

ESimProfile* Euicc::GetProfileFromPath(const dbus::ObjectPath& path) {
  for (auto& esim_profile : esim_profiles_) {
    if (esim_profile->path() == path) {
      return esim_profile.get();
    }
  }
  return nullptr;
}

void Euicc::PerformInstallProfileFromActivationCode(
    const std::string& activation_code,
    const std::string& confirmation_code,
    InstallProfileFromActivationCodeCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhibiting cellular device";
    std::move(callback).Run(mojom::ProfileInstallResult::kFailure,
                            mojo::NullRemote());
    return;
  }

  HermesEuiccClient::Get()->InstallProfileFromActivationCode(
      path_, activation_code, confirmation_code,
      base::BindOnce(&Euicc::OnProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(inhibit_lock)));
}

void Euicc::OnProfileInstallResult(
    InstallProfileFromActivationCodeCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status,
    const dbus::ObjectPath* profile_path) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing profile status="
                   << static_cast<int>(status);
    std::move(callback).Run(InstallResultFromStatus(status),
                            mojo::NullRemote());
    return;
  }

  install_calls_pending_connect_.emplace(*profile_path, std::move(callback));
  esim_manager_->cellular_connection_handler()
      ->PrepareNewlyInstalledCellularNetworkForConnection(
          path_, *profile_path, std::move(inhibit_lock),
          base::BindOnce(&Euicc::OnNewProfileEnableSuccess,
                         weak_ptr_factory_.GetWeakPtr(), *profile_path),
          base::BindOnce(&Euicc::OnPrepareCellularNetworkForConnectionFailure,
                         weak_ptr_factory_.GetWeakPtr(), *profile_path));
}

void Euicc::OnNewProfileEnableSuccess(const dbus::ObjectPath& profile_path,
                                      const std::string& service_path) {
  const NetworkState* network_state =
      esim_manager_->network_state_handler()->GetNetworkState(service_path);
  if (!network_state) {
    HandleNewProfileEnableFailure(profile_path,
                                  NetworkConnectionHandler::kErrorNotFound);
    return;
  }

  if (!network_state->IsConnectingOrConnected()) {
    // The connection could fail but the user will be notified of connection
    // failures separately.
    esim_manager_->network_connection_handler()->ConnectToNetwork(
        service_path,
        /*success_callback=*/base::DoNothing(),
        /*error_callback=*/base::DoNothing(),
        /*check_error_state=*/false, ConnectCallbackMode::ON_STARTED);
  }

  auto it = install_calls_pending_connect_.find(profile_path);
  if (it == install_calls_pending_connect_.end()) {
    NET_LOG(ERROR) << "ESim profile install callback missing after enable "
                      "success. profile_path="
                   << profile_path.value();
    return;
  }
  InstallProfileFromActivationCodeCallback callback = std::move(it->second);
  install_calls_pending_connect_.erase(it);

  ESimProfile* esim_profile = GetProfileFromPath(profile_path);
  if (!esim_profile) {
    // An ESimProfile may not exist for the newly created esim profile object
    // path if ESimProfileHandler has not updated profile lists yet. Save the
    // callback until an UpdateProfileList call creates an ESimProfile
    // object for this path
    install_calls_pending_create_.emplace(profile_path, std::move(callback));
    return;
  }
  std::move(callback).Run(mojom::ProfileInstallResult::kSuccess,
                          esim_profile->CreateRemote());
}

void Euicc::OnPrepareCellularNetworkForConnectionFailure(
    const dbus::ObjectPath& profile_path,
    const std::string& service_path,
    const std::string& error_name) {
  HandleNewProfileEnableFailure(profile_path, error_name);
}

void Euicc::HandleNewProfileEnableFailure(const dbus::ObjectPath& profile_path,
                                          const std::string& error_name) {
  NET_LOG(ERROR) << "Error enabling newly created profile path="
                 << profile_path.value() << " error_name=" << error_name;

  auto it = install_calls_pending_connect_.find(profile_path);
  if (it == install_calls_pending_connect_.end()) {
    NET_LOG(ERROR)
        << "ESim profile install callback missing after enable failure";
    return;
  }
  std::move(it->second)
      .Run(mojom::ProfileInstallResult::kFailure, mojo::NullRemote());
  install_calls_pending_connect_.erase(it);
}

void Euicc::PerformRequestPendingProfiles(
    RequestPendingProfilesCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhibiting cellular device";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  HermesEuiccClient::Get()->RequestPendingProfiles(
      path_, /*root_smds=*/std::string(),
      base::BindOnce(&Euicc::OnRequestPendingProfilesResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(inhibit_lock)));
}

void Euicc::OnRequestPendingProfilesResult(
    RequestPendingProfilesCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Request Pending events failed status="
                   << static_cast<int>(status);
  }
  std::move(callback).Run(status == HermesResponseStatus::kSuccess
                              ? mojom::ESimOperationResult::kSuccess
                              : mojom::ESimOperationResult::kFailure);
  // inhibit_lock goes out of scope and will uninhibit automatically.
}

mojom::ProfileInstallResult Euicc::GetPendingProfileInfoFromActivationCode(
    const std::string& activation_code,
    ESimProfile** profile_info) {
  const auto iter = base::ranges::find_if(
      esim_profiles_, [activation_code](const auto& esim_profile) -> bool {
        return esim_profile->properties()->activation_code == activation_code;
      });
  if (iter == esim_profiles_.end()) {
    NET_LOG(EVENT) << "Get pending profile with activation failed: No profile "
                      "with activation_code.";
    return mojom::ProfileInstallResult::kFailure;
  }
  *profile_info = iter->get();
  if ((*profile_info)->properties()->state != mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Get pending profile with activation code failed: Profile"
                      "is not in pending state.";
    return mojom::ProfileInstallResult::kFailure;
  }
  return mojom::ProfileInstallResult::kSuccess;
}

ESimProfile* Euicc::UpdateOrCreateESimProfile(
    const CellularESimProfile& esim_profile_state) {
  ESimProfile* esim_profile = GetProfileFromPath(esim_profile_state.path());
  if (esim_profile) {
    esim_profile->UpdateProperties(esim_profile_state, /*notify=*/true);
    return nullptr;
  }
  esim_profiles_.push_back(
      std::make_unique<ESimProfile>(esim_profile_state, this, esim_manager_));
  return esim_profiles_.back().get();
}

bool Euicc::RemoveUntrackedProfiles(
    const std::vector<CellularESimProfile>& esim_profile_states) {
  std::set<std::string> new_iccids;
  for (auto esim_profile_state : esim_profile_states) {
    if (esim_profile_state.eid() != properties_->eid) {
      continue;
    }
    new_iccids.insert(esim_profile_state.iccid());
  }

  bool removed = false;
  for (auto it = esim_profiles_.begin(); it != esim_profiles_.end();) {
    ESimProfile* profile = (*it).get();
    if (new_iccids.find(profile->properties()->iccid) == new_iccids.end()) {
      profile->OnProfileRemove();
      it = esim_profiles_.erase(it);
      removed = true;
    } else {
      it++;
    }
  }
  return removed;
}

}  // namespace cellular_setup
}  // namespace chromeos
