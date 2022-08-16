// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_state_handler.h"

#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace hotspot_config {
namespace mojom = ::chromeos::hotspot_config::mojom;
}  // namespace hotspot_config

namespace {

hotspot_config::mojom::HotspotState ShillTetheringStateToMojomState(
    const std::string& shill_state) {
  using HotspotState = hotspot_config::mojom::HotspotState;

  if (shill_state == shill::kTetheringStateActive) {
    return HotspotState::kEnabled;
  }

  if (shill_state == shill::kTetheringStateIdle) {
    return HotspotState::kDisabled;
  }

  if (shill_state == shill::kTetheringStateStarting) {
    return HotspotState::kEnabling;
  }

  if (shill_state == shill::kTetheringStateStopping) {
    return HotspotState::kDisabling;
  }

  NOTREACHED() << "Unexpeted shill tethering state: " << shill_state;
  return HotspotState::kDisabled;
}

size_t GetActiveClientCount(const base::Value& status) {
  const base::Value* active_clients =
      status.FindListKey(shill::kTetheringStatusClientsProperty);
  if (!active_clients) {
    NET_LOG(ERROR) << shill::kTetheringStatusClientsProperty << " not found in "
                   << shill::kTetheringStatusProperty;
    return 0;
  }
  return active_clients->GetList().size();
}

}  // namespace

HotspotStateHandler::HotspotStateHandler() = default;

HotspotStateHandler::~HotspotStateHandler() {
  if (ShillManagerClient::Get()) {
    ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
  }
}

void HotspotStateHandler::Init() {
  // Add as an observer here so that new hotspot state updated after this call
  // are recognized.
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&HotspotStateHandler::OnManagerProperties,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotStateHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HotspotStateHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool HotspotStateHandler::HasObserver(Observer* observer) const {
  return observer_list_.HasObserver(observer);
}

const hotspot_config::mojom::HotspotState&
HotspotStateHandler::GetHotspotState() const {
  return hotspot_state_;
}

size_t HotspotStateHandler::GetHotspotActiveClientCount() const {
  return active_client_count_;
}

void HotspotStateHandler::OnPropertyChanged(const std::string& key,
                                            const base::Value& value) {
  if (key == shill::kTetheringStatusProperty)
    UpdateHotspotStatus(value);
}

void HotspotStateHandler::OnManagerProperties(
    absl::optional<base::Value> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "HotspotStateHandler: Failed to get manager properties.";
    return;
  }

  const base::Value* status =
      properties->FindDictKey(shill::kTetheringStatusProperty);
  if (!status) {
    NET_LOG(EVENT) << "HotspotStateHandler: No dict value for: "
                   << shill::kTetheringStatusProperty;
    return;
  }
  UpdateHotspotStatus(*status);
}

void HotspotStateHandler::UpdateHotspotStatus(const base::Value& status) {
  const std::string* state =
      status.FindStringKey(shill::kTetheringStatusStateProperty);
  if (!state) {
    NET_LOG(EVENT) << "HotspotStateHandler: No string value for: "
                   << shill::kTetheringStatusStateProperty << " in "
                   << shill::kTetheringStatusProperty;
    return;
  }

  if (*state == shill::kTetheringStateFailure) {
    // Fall back to either idle or active state if the current state is enabling
    // or disabling.
    FallbackStateOnFailure();

    const std::string* error =
        status.FindStringKey(shill::kTetheringStatusErrorProperty);
    if (!error) {
      NET_LOG(ERROR)
          << "HotspotStateHandler: Failed to get hotspot status error.";
    } else {
      NET_LOG(ERROR) << "HotspotStateHandler: Hotspot status error: " << *error;
    }
    NotifyHotspotStateFailed(error ? *error : std::string());
    return;
  }

  hotspot_config::mojom::HotspotState mojom_state =
      ShillTetheringStateToMojomState(*state);
  if (mojom_state != hotspot_state_) {
    hotspot_state_ = mojom_state;
    NotifyHotspotStatusChanged();
  }

  if (mojom_state != hotspot_config::mojom::HotspotState::kEnabled) {
    active_client_count_ = 0;
    return;
  }
  size_t active_client_count = GetActiveClientCount(status);
  if (active_client_count == active_client_count_)
    return;

  active_client_count_ = active_client_count;
  NotifyHotspotStatusChanged();
}

void HotspotStateHandler::FallbackStateOnFailure() {
  using HotspotState = hotspot_config::mojom::HotspotState;
  if (hotspot_state_ == HotspotState::kEnabled ||
      hotspot_state_ == HotspotState::kDisabled) {
    return;
  }

  if (hotspot_state_ == HotspotState::kEnabling) {
    hotspot_state_ = HotspotState::kDisabled;
  } else if (hotspot_state_ == HotspotState::kDisabling) {
    hotspot_state_ = HotspotState::kEnabled;
  }
  NotifyHotspotStatusChanged();
}

void HotspotStateHandler::NotifyHotspotStatusChanged() {
  for (auto& observer : observer_list_)
    observer.OnHotspotStatusChanged();
}

void HotspotStateHandler::NotifyHotspotStateFailed(const std::string& error) {
  for (auto& observer : observer_list_)
    observer.OnHotspotStateFailed(error);
}

}  // namespace ash