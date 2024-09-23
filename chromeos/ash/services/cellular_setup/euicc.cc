// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/euicc.h"

#include <cstdint>
#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_esim_installer.h"
#include "chromeos/ash/components/network/cellular_esim_profile.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/hermes_metrics_util.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/cellular_setup/esim_manager.h"
#include "chromeos/ash/services/cellular_setup/esim_mojo_utils.h"
#include "chromeos/ash/services/cellular_setup/esim_profile.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-shared.h"
#include "components/device_event_log/device_event_log.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "dbus/object_path.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::cellular_setup {

namespace {


// Prefix for EID when encoded in QR Code.
const char kEidQrCodePrefix[] = "EID:";

CellularNetworkMetricsLogger::ESimUserInstallMethod ProfileInstallMethodToEnum(
    mojom::ProfileInstallMethod install_method) {
  using mojom::ProfileInstallMethod;
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
  };
}

}  // namespace

// static
void Euicc::RecordRequestPendingProfilesResult(
    RequestPendingProfilesResult result) {
  base::UmaHistogramEnumeration(
      "Network.Cellular.ESim.RequestPendingProfiles.OperationResult", result);
}

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
    mojom::ProfileInstallMethod install_method,
    InstallProfileFromActivationCodeCallback callback) {
  CellularNetworkMetricsLogger::LogESimUserInstallMethod(
      ProfileInstallMethodToEnum(install_method));

  esim_manager_->cellular_esim_installer()->InstallProfileFromActivationCode(
      activation_code, confirmation_code, path_,
      /*new_shill_properties=*/base::Value::Dict(),
      base::BindOnce(&Euicc::OnESimInstallProfileResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      /*is_initial_install=*/true, install_method);
}

void Euicc::OnESimInstallProfileResult(
    InstallProfileFromActivationCodeCallback callback,
    HermesResponseStatus hermes_status,
    std::optional<dbus::ObjectPath> profile_path,
    std::optional<std::string> /*service_path*/) {
  mojom::ProfileInstallResult status = InstallResultFromStatus(hermes_status);
  if (status != mojom::ProfileInstallResult::kSuccess) {
    std::move(callback).Run(status, mojo::NullRemote());
    return;
  }

  DCHECK(profile_path != std::nullopt);
  ESimProfile* esim_profile = GetProfileFromPath(profile_path.value());
  if (!esim_profile) {
    // An ESimProfile may not exist for the newly created esim profile object
    // path if ESimProfileHandler has not updated profile lists yet. Save the
    // callback until an UpdateProfileList call creates an ESimProfile
    // object for this path
    install_calls_pending_create_.emplace(profile_path.value(),
                                          std::move(callback));
    return;
  }
  std::move(callback).Run(mojom::ProfileInstallResult::kSuccess,
                          esim_profile->CreateRemote());
}

void Euicc::RequestAvailableProfiles(
    RequestAvailableProfilesCallback callback) {
  esim_manager_->cellular_esim_profile_handler()->RequestAvailableProfiles(
      path_,
      base::BindOnce(&Euicc::OnRequestAvailableProfiles,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void Euicc::RefreshInstalledProfiles(
    RefreshInstalledProfilesCallback callback) {
  NET_LOG(EVENT) << "Refreshing installed profiles";
  esim_manager_->cellular_esim_profile_handler()->RefreshProfileList(
      path_,
      base::BindOnce(
          [](RefreshInstalledProfilesCallback callback,
             std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
            std::move(callback).Run(inhibit_lock
                                        ? mojom::ESimOperationResult::kSuccess
                                        : mojom::ESimOperationResult::kFailure);
          },
          std::move(callback)));
}

void Euicc::GetEidQRCode(GetEidQRCodeCallback callback) {
  // Format EID to string that should be encoded in the QRCode.
  std::string qr_code_string =
      base::StrCat({kEidQrCodePrefix, properties_->eid});

  auto qr_data =
      qr_code_generator::GenerateCode(base::as_byte_span(qr_code_string));
  if (!qr_data.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Data returned from QR code generator consist of bytes that represents
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

void Euicc::OnRequestAvailableProfiles(
    RequestAvailableProfilesCallback callback,
    mojom::ESimOperationResult result,
    std::vector<CellularESimProfile> profile_list) {
  std::vector<mojom::ESimProfilePropertiesPtr> profile_properties_list;
  for (const auto& profile : profile_list) {
    mojom::ESimProfilePropertiesPtr properties =
        mojom::ESimProfileProperties::New();
    properties->eid = profile.eid();
    properties->iccid = profile.iccid();
    properties->name = profile.name();
    properties->nickname = profile.nickname();
    properties->service_provider = profile.service_provider();
    properties->state = ProfileStateToMojo(profile.state());
    properties->activation_code = profile.activation_code();
    profile_properties_list.push_back(std::move(properties));
  }
  std::move(callback).Run(result, std::move(profile_properties_list));
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

}  // namespace ash::cellular_setup
