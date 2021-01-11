// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/euicc.h"

#include <cstdint>
#include <memory>

#include "base/optional.h"
#include "base/strings/strcat.h"
#include "chromeos/services/cellular_setup/esim_mojo_utils.h"
#include "chromeos/services/cellular_setup/esim_profile.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-shared.h"
#include "components/device_event_log/device_event_log.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "dbus/object_path.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace {

// Prefix for EID when encoded in QR Code.
const char kEidQrCodePrefix[] = "EID:";

}  // namespace

namespace chromeos {
namespace cellular_setup {

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
  if (profile_info && status != mojom::ProfileInstallResult::kSuccess) {
    // Return early if profile was found but not in the correct state.
    std::move(callback).Run(status, mojo::NullRemote());
    return;
  }

  if (profile_info) {
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
  HermesEuiccClient::Get()->InstallProfileFromActivationCode(
      path_, activation_code, confirmation_code,
      base::BindOnce(&Euicc::OnProfileInstallResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void Euicc::RequestPendingProfiles(RequestPendingProfilesCallback callback) {
  NET_LOG(EVENT) << "Requesting Pending profiles";
  HermesEuiccClient::Get()->RequestPendingEvents(
      path_,
      base::BindOnce(&Euicc::OnRequestPendingEventsResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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

void Euicc::UpdateProfileList() {
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(path_);
  std::set<dbus::ObjectPath> new_profile_paths;
  for (auto& path : euicc_properties->installed_carrier_profiles().value()) {
    GetOrCreateESimProfile(path);
    new_profile_paths.insert(path);
  }
  for (auto& path : euicc_properties->pending_carrier_profiles().value()) {
    GetOrCreateESimProfile(path);
    new_profile_paths.insert(path);
  }
  RemoveUntrackedProfiles(new_profile_paths);
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

void Euicc::OnProfileInstallResult(
    InstallProfileFromActivationCodeCallback callback,
    HermesResponseStatus status,
    const dbus::ObjectPath* object_path) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Error Installing profile status="
                   << static_cast<int>(status);
    std::move(callback).Run(InstallResultFromStatus(status),
                            mojo::NullRemote());
    return;
  }

  ESimProfile* profile_info = GetOrCreateESimProfile(*object_path);
  std::move(callback).Run(mojom::ProfileInstallResult::kSuccess,
                          profile_info->CreateRemote());
}

void Euicc::OnRequestPendingEventsResult(
    RequestPendingProfilesCallback callback,
    HermesResponseStatus status) {
  if (status != HermesResponseStatus::kSuccess) {
    NET_LOG(ERROR) << "Request Pending events failed status="
                   << static_cast<int>(status);
  }
  std::move(callback).Run(status == HermesResponseStatus::kSuccess
                              ? mojom::ESimOperationResult::kSuccess
                              : mojom::ESimOperationResult::kFailure);
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

ESimProfile* Euicc::GetOrCreateESimProfile(
    const dbus::ObjectPath& carrier_profile_path) {
  ESimProfile* profile_info = GetProfileFromPath(carrier_profile_path);
  if (profile_info)
    return profile_info;
  esim_profiles_.push_back(
      std::make_unique<ESimProfile>(carrier_profile_path, this, esim_manager_));
  return esim_profiles_.back().get();
}

void Euicc::RemoveUntrackedProfiles(
    const std::set<dbus::ObjectPath>& new_profile_paths) {
  for (auto it = esim_profiles_.begin(); it != esim_profiles_.end();) {
    if (new_profile_paths.find((*it)->path()) == new_profile_paths.end()) {
      it = esim_profiles_.erase(it);
    } else {
      it++;
    }
  }
}

}  // namespace cellular_setup
}  // namespace chromeos