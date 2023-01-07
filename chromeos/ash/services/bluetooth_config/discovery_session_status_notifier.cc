// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/discovery_session_status_notifier.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "components/device_event_log/device_event_log.h"

namespace ash::bluetooth_config {

DiscoverySessionStatusNotifier::DiscoverySessionStatusNotifier() = default;

DiscoverySessionStatusNotifier::~DiscoverySessionStatusNotifier() = default;

void DiscoverySessionStatusNotifier::ObserveDiscoverySessionStatusChanges(
    mojo::PendingRemote<mojom::DiscoverySessionStatusObserver> observer) {
  observers_.Add(std::move(observer));
}

void DiscoverySessionStatusNotifier::NotifyHasAtLeastOneDiscoverySessionChanged(
    bool has_at_least_one_discovery_session) {
  BLUETOOTH_LOG(EVENT)
      << "Notifying observers that the existence of at least one"
         " discovery session has changed to: "
      << has_at_least_one_discovery_session;
  for (auto& observer : observers_) {
    observer->OnHasAtLeastOneDiscoverySessionChanged(
        has_at_least_one_discovery_session);
  }
}

}  // namespace ash::bluetooth_config
