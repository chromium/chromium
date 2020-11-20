// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/esim_profile.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/services/cellular_setup/esim_manager.h"
#include "chromeos/services/cellular_setup/esim_mojo_utils.h"
#include "chromeos/services/cellular_setup/euicc.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-shared.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"

namespace chromeos {
namespace cellular_setup {

ESimProfile::ESimProfile(const dbus::ObjectPath& path,
                         Euicc* euicc,
                         ESimManager* esim_manager)
    : euicc_(euicc),
      esim_manager_(esim_manager),
      properties_(mojom::ESimProfileProperties::New()),
      path_(path) {
  UpdateProperties();
  properties_->eid = euicc->properties()->eid;
}

ESimProfile::~ESimProfile() = default;

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

  HermesEuiccClient::Get()->UninstallProfile(
      euicc_->path(), path_,
      base::BindOnce(&ESimProfile::OnESimOperationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
  if (properties_->state == mojom::ProfileState::kInstalling ||
      properties_->state == mojom::ProfileState::kPending) {
    NET_LOG(ERROR) << "Set Profile Nickname failed: Profile is not installed.";
    std::move(callback).Run(mojom::ESimOperationResult::kFailure);
    return;
  }

  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(path_);
  properties->nick_name().Set(
      base::UTF16ToUTF8(nickname),
      base::BindOnce(&ESimProfile::OnProfilePropertySet,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ESimProfile::UpdateProperties() {
  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(path_);
  properties_->iccid = properties->iccid().value();
  properties_->name = base::UTF8ToUTF16(properties->name().value());
  properties_->nickname = base::UTF8ToUTF16(properties->nick_name().value());
  properties_->service_provider =
      base::UTF8ToUTF16(properties->service_provider().value());
  properties_->state = ProfileStateToMojo(properties->state().value());
  properties_->activation_code = properties->activation_code().value();
}

mojo::PendingRemote<mojom::ESimProfile> ESimProfile::CreateRemote() {
  mojo::PendingRemote<mojom::ESimProfile> esim_profile_remote;
  receiver_set_.Add(this, esim_profile_remote.InitWithNewPipeAndPassReceiver());
  return esim_profile_remote;
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

void ESimProfile::OnESimOperationResult(ESimOperationResultCallback callback,
                                        HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "ESim operation error status="
                   << static_cast<int>(status);
  }
  std::move(callback).Run(OperationResultFromStatus(status));
}

void ESimProfile::OnProfilePropertySet(ESimOperationResultCallback callback,
                                       bool success) {
  if (!success) {
    NET_LOG(ERROR) << "ESimProfile property set error.";
  }
  std::move(callback).Run(success ? mojom::ESimOperationResult::kSuccess
                                  : mojom::ESimOperationResult::kFailure);
}

}  // namespace cellular_setup
}  // namespace chromeos
