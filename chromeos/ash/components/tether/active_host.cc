// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/active_host.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/tether/pref_names.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::tether {

// static
std::string ActiveHost::StatusToString(const ActiveHostStatus& status) {
  switch (status) {
    case ActiveHostStatus::DISCONNECTED:
      return "DISCONNECTED";
    case ActiveHostStatus::CONNECTING:
      return "CONNECTING";
    case ActiveHostStatus::CONNECTED:
      return "CONNECTED";
  }
}

bool operator==(const ActiveHost::ActiveHostChangeInfo& first,
                const ActiveHost::ActiveHostChangeInfo& second) {
  bool new_devices_equal;
  if (first.new_active_host) {
    new_devices_equal = second.new_active_host &&
                        *first.new_active_host == *second.new_active_host;
  } else {
    new_devices_equal = !second.new_active_host;
  }

  return first.new_status == second.new_status &&
         first.old_status == second.old_status && new_devices_equal &&
         first.old_active_host_id == second.old_active_host_id &&
         first.new_tether_network_guid == second.new_tether_network_guid &&
         first.old_tether_network_guid == second.old_tether_network_guid &&
         first.new_wifi_network_guid == second.new_wifi_network_guid &&
         first.old_wifi_network_guid == second.old_wifi_network_guid;
}

ActiveHost::ActiveHostChangeInfo::ActiveHostChangeInfo()
    : new_status(ActiveHostStatus::DISCONNECTED),
      old_status(ActiveHostStatus::DISCONNECTED) {}

ActiveHost::ActiveHostChangeInfo::ActiveHostChangeInfo(
    ActiveHostStatus new_status,
    ActiveHostStatus old_status,
    std::optional<multidevice::RemoteDeviceRef> new_active_host,
    std::string old_active_host_id,
    std::string new_tether_network_guid,
    std::string old_tether_network_guid,
    std::string new_wifi_network_guid,
    std::string old_wifi_network_guid)
    : new_status(new_status),
      old_status(old_status),
      new_active_host(new_active_host),
      old_active_host_id(old_active_host_id),
      new_tether_network_guid(new_tether_network_guid),
      old_tether_network_guid(old_tether_network_guid),
      new_wifi_network_guid(new_wifi_network_guid),
      old_wifi_network_guid(old_wifi_network_guid) {}

ActiveHost::ActiveHostChangeInfo::ActiveHostChangeInfo(
    const ActiveHostChangeInfo& other) = default;

ActiveHost::ActiveHostChangeInfo::~ActiveHostChangeInfo() = default;

ActiveHost::ActiveHost(TetherHostFetcher* tether_host_fetcher,
                       PrefService* pref_service)
    : tether_host_fetcher_(tether_host_fetcher), pref_service_(pref_service) {}

ActiveHost::~ActiveHost() = default;

// static
void ActiveHost::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kActiveHostStatus,
      static_cast<int>(ActiveHostStatus::DISCONNECTED));
  registry->RegisterStringPref(prefs::kActiveHostDeviceId, "");
  registry->RegisterStringPref(prefs::kTetherNetworkGuid, "");
  registry->RegisterStringPref(prefs::kWifiNetworkGuid, "");
}

void ActiveHost::SetActiveHostDisconnected() {
  SetActiveHost(ActiveHostStatus::DISCONNECTED, "" /* active_host_device_id */,
                "" /* tether_network_guid */, "" /* wifi_network_guid */);
}

void ActiveHost::SetActiveHostConnecting(
    const std::string& active_host_device_id,
    const std::string& tether_network_guid) {
  DCHECK(!active_host_device_id.empty());

  SetActiveHost(ActiveHostStatus::CONNECTING, active_host_device_id,
                tether_network_guid, "" /* wifi_network_guid */);
}

void ActiveHost::SetActiveHostConnected(
    const std::string& active_host_device_id,
    const std::string& tether_network_guid,
    const std::string& wifi_network_guid) {
  DCHECK(!active_host_device_id.empty());
  DCHECK(!tether_network_guid.empty());
  DCHECK(!wifi_network_guid.empty());

  SetActiveHost(ActiveHostStatus::CONNECTED, active_host_device_id,
                tether_network_guid, wifi_network_guid);
}

void ActiveHost::GetActiveHost(ActiveHostCallback active_host_callback) {
  ActiveHostStatus status = GetActiveHostStatus();

  if (status == ActiveHostStatus::DISCONNECTED) {
    std::move(active_host_callback)
        .Run(status, std::nullopt /* active_host */,
             "" /* tether_network_guid */, "" /* wifi_network_guid */);
    return;
  }

  std::string active_host_device_id = GetActiveHostDeviceId();
  DCHECK(!active_host_device_id.empty());

  std::optional<multidevice::RemoteDeviceRef> active_host =
      tether_host_fetcher_->GetTetherHost();
  if (GetActiveHostDeviceId().empty() || !active_host) {
    DCHECK(GetActiveHostStatus() == ActiveHostStatus::DISCONNECTED);
    DCHECK(GetTetherNetworkGuid().empty());
    DCHECK(GetWifiNetworkGuid().empty());

    // If the active host became disconnected while the tether host was being
    // fetched, forward this information to the callback.
    std::move(active_host_callback)
        .Run(ActiveHostStatus::DISCONNECTED, std::nullopt /* active_host */,
             "" /* wifi_network_guid */, "" /* tether_network_guid */);
    return;
  }

  if (GetActiveHostDeviceId() != active_host->GetDeviceId()) {
    // If the active host has changed while the tether host was being fetched,
    // perform the fetch again.
    GetActiveHost(std::move(active_host_callback));
    return;
  }

  if (GetActiveHostStatus() == ActiveHostStatus::CONNECTING) {
    DCHECK(!GetTetherNetworkGuid().empty());
    DCHECK(GetWifiNetworkGuid().empty());
    std::move(active_host_callback)
        .Run(ActiveHostStatus::CONNECTING, active_host,
             GetTetherNetworkGuid() /* tether_network_guid */,
             "" /* wifi_network_guid */);
    return;
  }

  DCHECK(GetActiveHostStatus() == ActiveHostStatus::CONNECTED);
  DCHECK(!GetTetherNetworkGuid().empty());
  DCHECK(!GetWifiNetworkGuid().empty());
  std::move(active_host_callback)
      .Run(ActiveHostStatus::CONNECTED, active_host, GetTetherNetworkGuid(),
           GetWifiNetworkGuid());
}

ActiveHost::ActiveHostStatus ActiveHost::GetActiveHostStatus() const {
  return static_cast<ActiveHostStatus>(
      pref_service_->GetInteger(prefs::kActiveHostStatus));
}

std::string ActiveHost::GetActiveHostDeviceId() const {
  return pref_service_->GetString(prefs::kActiveHostDeviceId);
}

std::string ActiveHost::GetWifiNetworkGuid() const {
  return pref_service_->GetString(prefs::kWifiNetworkGuid);
}

std::string ActiveHost::GetTetherNetworkGuid() const {
  return pref_service_->GetString(prefs::kTetherNetworkGuid);
}

void ActiveHost::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ActiveHost::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ActiveHost::SetActiveHost(ActiveHostStatus active_host_status,
                               const std::string& active_host_device_id,
                               const std::string& tether_network_guid,
                               const std::string& wifi_network_guid) {
  ActiveHostStatus old_status = GetActiveHostStatus();
  std::string old_device_id = GetActiveHostDeviceId();
  std::string old_tether_network_guid = GetTetherNetworkGuid();
  std::string old_wifi_network_guid = GetWifiNetworkGuid();

  if (old_status == active_host_status &&
      old_device_id == active_host_device_id &&
      old_tether_network_guid == tether_network_guid &&
      old_wifi_network_guid == wifi_network_guid) {
    // If nothing has changed, return early.
    return;
  }

  pref_service_->Set(prefs::kActiveHostStatus,
                     base::Value(static_cast<int>(active_host_status)));
  pref_service_->Set(prefs::kActiveHostDeviceId,
                     base::Value(active_host_device_id));
  pref_service_->Set(prefs::kTetherNetworkGuid,
                     base::Value(tether_network_guid));
  pref_service_->Set(prefs::kWifiNetworkGuid, base::Value(wifi_network_guid));

  // Now, send an active host changed update.
  GetActiveHost(base::BindOnce(&ActiveHost::SendActiveHostChangedUpdate,
                               weak_ptr_factory_.GetWeakPtr(), old_status,
                               old_device_id, old_tether_network_guid,
                               old_wifi_network_guid));
}

void ActiveHost::SendActiveHostChangedUpdate(
    ActiveHostStatus old_status,
    const std::string& old_active_host_id,
    const std::string& old_tether_network_guid,
    const std::string& old_wifi_network_guid,
    ActiveHostStatus new_status,
    std::optional<multidevice::RemoteDeviceRef> new_active_host,
    const std::string& new_tether_network_guid,
    const std::string& new_wifi_network_guid) {
  PA_LOG(INFO) << "Active host changed from " << static_cast<int>(old_status)
               << " to " << static_cast<int>(new_status);
  ActiveHostChangeInfo info;
  info.new_status = new_status;
  info.old_status = old_status;
  info.new_active_host = new_active_host;
  info.old_active_host_id = old_active_host_id;
  info.old_tether_network_guid = old_tether_network_guid;
  info.new_tether_network_guid = new_tether_network_guid;
  info.new_wifi_network_guid = new_wifi_network_guid;
  info.old_wifi_network_guid = old_wifi_network_guid;

  for (auto& observer : observer_list_) {
    PA_LOG(INFO) << "Notifying observer of active host change";
    observer.OnActiveHostChanged(info);
  }
}

}  // namespace ash::tether
