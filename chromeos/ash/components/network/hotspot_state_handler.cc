// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_state_handler.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_util.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

size_t GetActiveClientCount(const base::Value::Dict& status) {
  const base::Value::List* active_clients =
      status.FindList(shill::kTetheringStatusClientsProperty);
  if (!active_clients) {
    NET_LOG(ERROR) << shill::kTetheringStatusClientsProperty << " not found in "
                   << shill::kTetheringStatusProperty;
    return 0;
  }
  return active_clients->size();
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

const absl::optional<hotspot_config::mojom::DisableReason>
HotspotStateHandler::GetDisableReason() const {
  return disable_reason_;
}

size_t HotspotStateHandler::GetHotspotActiveClientCount() const {
  return active_client_count_;
}

void HotspotStateHandler::OnPropertyChanged(const std::string& key,
                                            const base::Value& value) {
  if (key == shill::kTetheringStatusProperty) {
    UpdateHotspotStatus(value.GetDict());
  }
}

void HotspotStateHandler::OnManagerProperties(
    absl::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "HotspotStateHandler: Failed to get manager properties.";
    return;
  }

  const base::Value::Dict* status =
      properties->FindDict(shill::kTetheringStatusProperty);
  if (!status) {
    NET_LOG(EVENT) << "HotspotStateHandler: No dict value for: "
                   << shill::kTetheringStatusProperty;
  } else {
    UpdateHotspotStatus(*status);
  }
}

void HotspotStateHandler::UpdateHotspotStatus(const base::Value::Dict& status) {
  const std::string* state =
      status.FindString(shill::kTetheringStatusStateProperty);
  if (!state) {
    NET_LOG(EVENT) << "HotspotStateHandler: No string value for: "
                   << shill::kTetheringStatusStateProperty << " in "
                   << shill::kTetheringStatusProperty;
    return;
  }

  hotspot_config::mojom::HotspotState mojom_state =
      ShillTetheringStateToMojomState(*state);
  if (mojom_state != hotspot_state_) {
    hotspot_state_ = mojom_state;
    UpdateDisableReason(status);
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

void HotspotStateHandler::UpdateDisableReason(const base::Value::Dict& status) {
  const std::string* idle_reason =
      status.FindString(shill::kTetheringStatusIdleReasonProperty);
  if (!idle_reason) {
    disable_reason_ = absl::nullopt;
    NET_LOG(EVENT) << "HotspotStateHandler: No string value for: "
                   << shill::kTetheringStatusIdleReasonProperty << " in "
                   << shill::kTetheringStatusProperty;
    return;
  }

  if (*idle_reason != shill::kTetheringIdleReasonInitialState) {
    hotspot_config::mojom::DisableReason disable_reason =
        ShillTetheringIdleReasonToMojomState(*idle_reason);
    disable_reason_ = disable_reason;
  }
}

void HotspotStateHandler::NotifyHotspotStatusChanged() {
  for (auto& observer : observer_list_)
    observer.OnHotspotStatusChanged();
}

}  // namespace ash
