// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/esim_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/cellular_setup/esim_profile.h"
#include "chromeos/services/cellular_setup/euicc.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace chromeos {
namespace cellular_setup {

ESimManager::ESimManager() {
  HermesManagerClient::Get()->AddObserver(this);
  HermesEuiccClient::Get()->AddObserver(this);
  HermesProfileClient::Get()->AddObserver(this);
  UpdateAvailableEuiccs();
}

ESimManager::~ESimManager() {
  HermesManagerClient::Get()->RemoveObserver(this);
  HermesEuiccClient::Get()->RemoveObserver(this);
  HermesProfileClient::Get()->RemoveObserver(this);
}

void ESimManager::AddObserver(
    mojo::PendingRemote<mojom::ESimManagerObserver> observer) {
  observers_.Add(std::move(observer));
}

void ESimManager::GetAvailableEuiccs(GetAvailableEuiccsCallback callback) {
  std::vector<mojo::PendingRemote<mojom::Euicc>> euicc_list;
  for (auto const& euicc : available_euiccs_)
    euicc_list.push_back(euicc->CreateRemote());
  std::move(callback).Run(std::move(euicc_list));
}

void ESimManager::OnAvailableEuiccListChanged() {
  UpdateAvailableEuiccs();
  for (auto& observer : observers_)
    observer->OnAvailableEuiccListChanged();
}

void ESimManager::OnEuiccPropertyChanged(const dbus::ObjectPath& euicc_path,
                                         const std::string& property_name) {
  Euicc* euicc = GetEuiccFromPath(euicc_path);
  // Skip notifying observers if the euicc object is not tracked.
  if (!euicc)
    return;
  if (property_name == hermes::euicc::kPendingProfilesProperty ||
      property_name == hermes::euicc::kInstalledProfilesProperty) {
    euicc->UpdateProfileList();
    for (auto& observer : observers_)
      observer->OnProfileListChanged(euicc->CreateRemote());
  } else {
    euicc->UpdateProperties();
    for (auto& observer : observers_)
      observer->OnEuiccChanged(euicc->CreateRemote());
  }
}

void ESimManager::OnCarrierProfilePropertyChanged(
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& property_name) {
  ESimProfile* esim_profile = GetESimProfileFromPath(carrier_profile_path);

  // Skip notifying observers if the carrier profile is not tracked.
  if (!esim_profile)
    return;

  esim_profile->UpdateProperties();
  NotifyESimProfileChanged(esim_profile);
}

void ESimManager::BindReceiver(
    mojo::PendingReceiver<mojom::ESimManager> receiver) {
  NET_LOG(EVENT) << "ESimManager::BindReceiver()";
  receivers_.Add(this, std::move(receiver));
}

void ESimManager::NotifyESimProfileChanged(ESimProfile* esim_profile) {
  for (auto& observer : observers_)
    observer->OnProfileChanged(esim_profile->CreateRemote());
}

void ESimManager::UpdateAvailableEuiccs() {
  NET_LOG(EVENT) << "Updating available Euiccs";
  std::set<dbus::ObjectPath> new_euicc_paths;
  for (auto& euicc_path : HermesManagerClient::Get()->GetAvailableEuiccs()) {
    Euicc* euicc_info = GetOrCreateEuicc(euicc_path);
    euicc_info->UpdateProfileList();
    new_euicc_paths.insert(euicc_path);
  }
  RemoveUntrackedEuiccs(new_euicc_paths);
}

void ESimManager::RemoveUntrackedEuiccs(
    const std::set<dbus::ObjectPath> new_euicc_paths) {
  for (auto euicc_it = available_euiccs_.begin();
       euicc_it != available_euiccs_.end();) {
    if (new_euicc_paths.find((*euicc_it)->path()) == new_euicc_paths.end()) {
      euicc_it = available_euiccs_.erase(euicc_it);
    } else {
      euicc_it++;
    }
  }
}

Euicc* ESimManager::GetOrCreateEuicc(const dbus::ObjectPath& euicc_path) {
  Euicc* euicc_info = GetEuiccFromPath(euicc_path);
  if (euicc_info)
    return euicc_info;
  available_euiccs_.push_back(std::make_unique<Euicc>(euicc_path, this));
  return available_euiccs_.back().get();
}

Euicc* ESimManager::GetEuiccFromPath(const dbus::ObjectPath& path) {
  for (auto& euicc : available_euiccs_) {
    if (euicc->path() == path) {
      return euicc.get();
    }
  }
  return nullptr;
}

ESimProfile* ESimManager::GetESimProfileFromPath(const dbus::ObjectPath& path) {
  for (auto& euicc : available_euiccs_) {
    ESimProfile* esim_profile = euicc->GetProfileFromPath(path);
    if (esim_profile) {
      return esim_profile;
    }
  }
  return nullptr;
}

}  // namespace cellular_setup
}  // namespace chromeos
