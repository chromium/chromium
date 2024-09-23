// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/esim_manager.h"

#include <sstream>

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/services/cellular_setup/esim_profile.h"
#include "chromeos/ash/services/cellular_setup/euicc.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash::cellular_setup {

namespace {

// The Stork SM-DS Prod server used to fetch pending ESim profiles.
const char kStorkSmdsServerAddress[] = "1$prod.smds.rsp.goog$";

void LogEuiccPaths(const std::set<dbus::ObjectPath>& new_euicc_paths) {
  if (new_euicc_paths.empty()) {
    NET_LOG(EVENT) << "EUICC list updated; no EUICCs present";
    return;
  }

  std::stringstream ss("[");
  for (const auto& new_path : new_euicc_paths)
    ss << new_path.value() << ", ";
  ss.seekp(-2, ss.cur);  // Remove last ", " from the stream.
  ss << "]";

  NET_LOG(EVENT) << "EUICC list updated; paths: " << ss.str();
}

}  // namespace

// static
std::string ESimManager::GetRootSmdsAddress() {
  // This function returns which server should be used when performing an SM-DS
  // scan and will only ever return the root GSM Association server or the Stork
  // server.
  return features::ShouldUseStorkSmds() ? kStorkSmdsServerAddress
                                        : std::string();
}

ESimManager::ESimManager()
    : ESimManager(NetworkHandler::Get()->cellular_connection_handler(),
                  NetworkHandler::Get()->cellular_esim_installer(),
                  NetworkHandler::Get()->cellular_esim_profile_handler(),
                  NetworkHandler::Get()->cellular_esim_uninstall_handler(),
                  NetworkHandler::Get()->cellular_inhibitor(),
                  NetworkHandler::Get()->network_connection_handler(),
                  NetworkHandler::Get()->network_state_handler()) {}

ESimManager::ESimManager(
    CellularConnectionHandler* cellular_connection_handler,
    CellularESimInstaller* cellular_esim_installer,
    CellularESimProfileHandler* cellular_esim_profile_handler,
    CellularESimUninstallHandler* cellular_esim_uninstall_handler,
    CellularInhibitor* cellular_inhibitor,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler)
    : cellular_connection_handler_(cellular_connection_handler),
      cellular_esim_installer_(cellular_esim_installer),
      cellular_esim_profile_handler_(cellular_esim_profile_handler),
      cellular_esim_uninstall_handler_(cellular_esim_uninstall_handler),
      cellular_inhibitor_(cellular_inhibitor),
      network_connection_handler_(network_connection_handler),
      network_state_handler_(network_state_handler) {
  HermesManagerClient::Get()->AddObserver(this);
  HermesEuiccClient::Get()->AddObserver(this);
  cellular_esim_profile_handler_->AddObserver(this);
  OnESimProfileListUpdated();
}

ESimManager::~ESimManager() {
  HermesManagerClient::Get()->RemoveObserver(this);
  HermesEuiccClient::Get()->RemoveObserver(this);
  cellular_esim_profile_handler_->RemoveObserver(this);
}

void ESimManager::AddObserver(
    mojo::PendingRemote<mojom::ESimManagerObserver> observer) {
  observers_.Add(std::move(observer));
}

void ESimManager::GetAvailableEuiccs(GetAvailableEuiccsCallback callback) {
  std::vector<mojo::PendingRemote<mojom::Euicc>> euicc_list;
  NET_LOG(DEBUG) << "GetAvailableEuiccs(): Num available_euiccs_: "
                 << available_euiccs_.size();
  for (auto const& euicc : available_euiccs_)
    euicc_list.push_back(euicc->CreateRemote());
  std::move(callback).Run(std::move(euicc_list));
}

void ESimManager::OnAvailableEuiccListChanged() {
  UpdateAvailableEuiccs();
}

void ESimManager::OnEuiccPropertyChanged(const dbus::ObjectPath& euicc_path,
                                         const std::string& property_name) {
  Euicc* euicc = GetEuiccFromPath(euicc_path);
  // Skip notifying observers if the euicc object is not tracked.
  if (!euicc)
    return;

  // Profile lists are handled in OnESimProfileListUpdated notification from
  // CellularESimProfileHandler.
  if (property_name == hermes::euicc::kPendingProfilesProperty ||
      property_name == hermes::euicc::kInstalledProfilesProperty) {
    return;
  }
  euicc->UpdateProperties();
  for (auto& observer : observers_)
    observer->OnEuiccChanged(euicc->CreateRemote());
}

void ESimManager::OnESimProfileListUpdated() {
  // Force update available euiccs in case OnAvailableEuiccListChanged on this
  // class was not called before this handler
  UpdateAvailableEuiccs();

  std::vector<CellularESimProfile> esim_profile_states =
      cellular_esim_profile_handler_->GetESimProfiles();
  for (auto& euicc : available_euiccs_) {
    euicc->UpdateProfileList(esim_profile_states);
  }
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

void ESimManager::NotifyESimProfileListChanged(Euicc* euicc) {
  for (auto& observer : observers_)
    observer->OnProfileListChanged(euicc->CreateRemote());
}

void ESimManager::UpdateAvailableEuiccs() {
  NET_LOG(DEBUG) << "Updating available Euiccs";

  std::set<dbus::ObjectPath> new_euicc_paths;
  bool available_euiccs_changed = false;

  for (auto& euicc_path : HermesManagerClient::Get()->GetAvailableEuiccs()) {
    available_euiccs_changed |= CreateEuiccIfNew(euicc_path);
    new_euicc_paths.insert(euicc_path);
  }

  available_euiccs_changed |= RemoveUntrackedEuiccs(new_euicc_paths);

  if (!available_euiccs_changed)
    return;

  LogEuiccPaths(new_euicc_paths);

  for (auto& observer : observers_)
    observer->OnAvailableEuiccListChanged();
}

bool ESimManager::RemoveUntrackedEuiccs(
    const std::set<dbus::ObjectPath> new_euicc_paths) {
  bool removed = false;
  for (auto euicc_it = available_euiccs_.begin();
       euicc_it != available_euiccs_.end();) {
    if (new_euicc_paths.find((*euicc_it)->path()) == new_euicc_paths.end()) {
      removed = true;
      NET_LOG(DEBUG) << "Removing EUICC: " << (*euicc_it)->path().value();
      euicc_it = available_euiccs_.erase(euicc_it);
    } else {
      euicc_it++;
    }
  }
  return removed;
}

bool ESimManager::CreateEuiccIfNew(const dbus::ObjectPath& euicc_path) {
  Euicc* euicc_info = GetEuiccFromPath(euicc_path);
  if (euicc_info)
    return false;
  NET_LOG(DEBUG) << "Creating EUICC: " << euicc_path.value();
  available_euiccs_.push_back(std::make_unique<Euicc>(euicc_path, this));
  return true;
}

Euicc* ESimManager::GetEuiccFromPath(const dbus::ObjectPath& path) {
  for (auto& euicc : available_euiccs_) {
    if (euicc->path() == path) {
      return euicc.get();
    }
  }
  return nullptr;
}

}  // namespace ash::cellular_setup
