// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/esim_profile.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/network/cellular_esim_profile.h"
#include "chromeos/network/cellular_esim_uninstall_handler.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/services/cellular_setup/esim_manager.h"
#include "chromeos/services/cellular_setup/esim_mojo_utils.h"
#include "chromeos/services/cellular_setup/euicc.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-shared.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"

namespace chromeos {
namespace cellular_setup {

namespace {

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
  if (uninstall_callback_ || set_profile_nickname_callback_) {
    NET_LOG(ERROR) << "Profile destroyed with unfulfilled callbacks";
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

  HermesEuiccClient::Get()->InstallPendingProfile(
      euicc_->path(), path_, confirmation_code,
      base::BindOnce(&ESimProfile::OnPendingProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimProfile::UninstallProfile(UninstallProfileCallback callback) {
  if (properties_->state == mojom::ProfileState::kInstalling ||
      properties_->state == mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Profile uninstall failed: Profile is not installed.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  uninstall_callback_ = std::move(callback);
  esim_manager_->cellular_esim_uninstall_handler()->UninstallESim(
      properties_->iccid, path_, euicc_->path(),
      base::BindOnce(&ESimProfile::OnProfileUninstallResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ESimProfile::EnableProfile(EnableProfileCallback callback) {
  if (properties_->state == mojom::ProfileState::kActive ||
      properties_->state == mojom::ProfileState::kPending) {
    NET_LOG(ERROR)
        << "Profile enable failed: Profile already enabled or not installed";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  HermesProfileClient::Get()->EnableCarrierProfile(
      path_,
      base::BindOnce(&ESimProfile::OnESimOperationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimProfile::DisableProfile(DisableProfileCallback callback) {
  if (properties_->state == mojom::ProfileState::kInactive ||
      properties_->state == mojom::ProfileState::kPending) {
    NET_LOG(ERROR)
        << "Profile enable failed: Profile already disabled or not installed";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  HermesProfileClient::Get()->DisableCarrierProfile(
      path_,
      base::BindOnce(&ESimProfile::OnESimOperationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimProfile::SetProfileNickname(const base::string16& nickname,
                                     SetProfileNicknameCallback callback) {
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

  set_profile_nickname_callback_ = std::move(callback);
  // Pass has_already_requested_installed_profiles=false so that a
  // RequestInstalledProfiles call will be made in case this ESimProfile object
  // was created from cached profile data.
  esim_manager_->cellular_inhibitor()->InhibitCellularScanning(base::BindOnce(
      &ESimProfile::PerformSetProfileNickname, weak_ptr_factory_.GetWeakPtr(),
      nickname, /*has_already_requested_installed_profiles=*/false));
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
  }
}

void ESimProfile::OnProfileRemove() {
  // Run pending callbacks before profile is removed.
  if (uninstall_callback_) {
    // This profile could be removed before UninstallHandler returns. Return a
    // success since the profile will be removed.
    std::move(uninstall_callback_).Run(mojom::ESimOperationResult::kSuccess);
  }
  if (set_profile_nickname_callback_) {
    // Setting nickname could trigger a request for installed profiles. If this
    // profile gets removed at that point, return pending call with failure
    // result.
    std::move(set_profile_nickname_callback_)
        .Run(mojom::ESimOperationResult::kFailure);
  }
}

mojo::PendingRemote<mojom::ESimProfile> ESimProfile::CreateRemote() {
  mojo::PendingRemote<mojom::ESimProfile> esim_profile_remote;
  receiver_set_.Add(this, esim_profile_remote.InitWithNewPipeAndPassReceiver());
  return esim_profile_remote;
}

void ESimProfile::PerformSetProfileNickname(
    const base::string16& nickname,
    bool has_already_requested_installed_profiles,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    NET_LOG(ERROR) << "Error inhibiting cellular device";
    std::move(set_profile_nickname_callback_)
        .Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  // If a valid profile does not exist on Hermes, then we are using cached
  // profile data. Make a request for installed profiles so that the profile
  // will be loaded in hermes.
  if (!ProfileExistsOnEuicc()) {
    if (has_already_requested_installed_profiles) {
      // If the profile doesn't exists and installed profiles have already been
      // requested then give up and return with error. This profile will get
      // destroyed when ESimProfileHandler updates.
      NET_LOG(ERROR) << "Unable to find profile in dbus. path="
                     << path_.value();
      std::move(set_profile_nickname_callback_)
          .Run(mojom::ESimOperationResult::kFailure);
      return;
    }

    HermesEuiccClient::Get()->RequestInstalledProfiles(
        euicc_->path(),
        base::BindOnce(&ESimProfile::OnRequestInstalledProfilesForSetNickname,
                       weak_ptr_factory_.GetWeakPtr(), nickname,
                       std::move(inhibit_lock)));
    return;
  }

  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(path_);
  properties->nick_name().Set(
      base::UTF16ToUTF8(nickname),
      base::BindOnce(&ESimProfile::OnProfileNicknameSet,
                     weak_ptr_factory_.GetWeakPtr(), std::move(inhibit_lock)));
}

void ESimProfile::OnRequestInstalledProfilesForSetNickname(
    const base::string16& nickname,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR)
        << "Error requesting installed profiles for set nickname. status="
        << static_cast<int>(status);
    std::move(set_profile_nickname_callback_)
        .Run(mojom::ESimOperationResult::kFailure);
    return;
  }
  PerformSetProfileNickname(nickname,
                            /*has_already_requested_intalled_profiles=*/true,
                            std::move(inhibit_lock));
}

void ESimProfile::OnPendingProfileInstallResult(
    ProfileInstallResultCallback callback,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing pending profile status="
                   << static_cast<int>(status);
    properties_->state = mojom::ProfileState::kPending;
    esim_manager_->NotifyESimProfileChanged(this);
    std::move(callback).Run(InstallResultFromStatus(status));
    return;
  }

  std::move(callback).Run(mojom::ProfileInstallResult::kSuccess);
}

void ESimProfile::OnProfileUninstallResult(bool success) {
  std::move(uninstall_callback_)
      .Run(success ? mojom::ESimOperationResult::kSuccess
                   : mojom::ESimOperationResult::kFailure);
}

void ESimProfile::OnESimOperationResult(ESimOperationResultCallback callback,
                                        HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "ESim operation error status="
                   << static_cast<int>(status);
  }
  std::move(callback).Run(OperationResultFromStatus(status));
}

void ESimProfile::OnProfileNicknameSet(
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    bool success) {
  if (!success) {
    NET_LOG(ERROR) << "ESimProfile property set error.";
  }
  std::move(set_profile_nickname_callback_)
      .Run(success ? mojom::ESimOperationResult::kSuccess
                   : mojom::ESimOperationResult::kFailure);
  // inhibit_lock goes out of scope and will uninhibit automatically.
}

bool ESimProfile::ProfileExistsOnEuicc() {
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_->path());
  const std::vector<dbus::ObjectPath>& installed_profile_paths =
      euicc_properties->installed_carrier_profiles().value();
  auto iter = std::find(installed_profile_paths.begin(),
                        installed_profile_paths.end(), path_);
  return iter != installed_profile_paths.end();
}

}  // namespace cellular_setup
}  // namespace chromeos
