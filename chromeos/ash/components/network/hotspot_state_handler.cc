// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_state_handler.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_util.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/dbus/power/power_policy_controller.h"
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

HotspotStateHandler::ActiveClientCount::ActiveClientCount() = default;

HotspotStateHandler::ActiveClientCount::~ActiveClientCount() {
  DisableWakeLock();
}

void HotspotStateHandler::ActiveClientCount::Set(size_t value) {
  value_ = value;
  if (value_ > 0) {
    EnableWakeLock();
  } else {
    DisableWakeLock();
  }
}

size_t HotspotStateHandler::ActiveClientCount::Get() const {
  return value_;
}

void HotspotStateHandler::ActiveClientCount::EnableWakeLock() {
  if (!wake_lock_id_.has_value()) {
    NET_LOG(EVENT) << "Enable wake lock";
    wake_lock_id_ = chromeos::PowerPolicyController::Get()->AddSystemWakeLock(
        chromeos::PowerPolicyController::WakeLockReason::REASON_OTHER,
        "Clients connected to hotspot");
  }
}

void HotspotStateHandler::ActiveClientCount::DisableWakeLock() {
  if (wake_lock_id_.has_value()) {
    NET_LOG(EVENT) << "Disable wake lock";
    chromeos::PowerPolicyController::Get()->RemoveWakeLock(*wake_lock_id_);
    wake_lock_id_.reset();
  }
}

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

const std::optional<hotspot_config::mojom::DisableReason>
HotspotStateHandler::GetDisableReason() const {
  return disable_reason_;
}

size_t HotspotStateHandler::GetHotspotActiveClientCount() const {
  return active_client_count_.Get();
}

void HotspotStateHandler::OnPropertyChanged(const std::string& key,
                                            const base::Value& value) {
  if (key == shill::kTetheringStatusProperty) {
    UpdateHotspotStatus(value.GetDict());
  }
}

void HotspotStateHandler::OnManagerProperties(
    std::optional<base::Value::Dict> properties) {
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
    active_client_count_.Set(0);
    return;
  }
  size_t active_client_count = GetActiveClientCount(status);
  if (active_client_count == active_client_count_.Get()) {
    return;
  }

  active_client_count_.Set(active_client_count);

  NotifyHotspotStatusChanged();
}

void HotspotStateHandler::UpdateDisableReason(const base::Value::Dict& status) {
  const std::string* idle_reason =
      status.FindString(shill::kTetheringStatusIdleReasonProperty);
  if (!idle_reason) {
    disable_reason_ = std::nullopt;
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
  for (auto& observer : observer_list_) {
    observer.OnHotspotStatusChanged();
  }
}

}  // namespace ash
