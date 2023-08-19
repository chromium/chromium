// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"

namespace ash::nearby::presence {

NearbyPresenceService::NearbyPresenceService() = default;
NearbyPresenceService::~NearbyPresenceService() = default;

NearbyPresenceService::ScanFilter::ScanFilter(
    IdentityType identity_type,
    const std::vector<Action>& actions)
    : identity_type_(identity_type), actions_(actions) {}

NearbyPresenceService::ScanFilter::~ScanFilter() = default;

NearbyPresenceService::ScanFilter::ScanFilter(const ScanFilter& scan_filter) {
  identity_type_ = scan_filter.identity_type_;
  actions_ = scan_filter.actions_;
}

NearbyPresenceService::ScanDelegate::ScanDelegate() = default;
NearbyPresenceService::ScanDelegate::~ScanDelegate() = default;

NearbyPresenceService::PresenceDevice::PresenceDevice(
    ::nearby::internal::DeviceType device_type,
    absl::optional<std::string> stable_device_id,
    std::string endpoint_id,
    std::string device_name,
    std::vector<Action> actions,
    int rssi)
    : device_type_(device_type),
      stable_device_id_(stable_device_id),
      endpoint_id_(endpoint_id),
      device_name_(device_name),
      actions_(actions),
      rssi_(rssi) {}

NearbyPresenceService::PresenceDevice::~PresenceDevice() = default;

NearbyPresenceService::ScanSession::ScanSession(
    mojo::PendingRemote<ash::nearby::presence::mojom::ScanSession>
        pending_remote,
    base::OnceClosure on_disconnect_callback)
    : remote_(std::move(pending_remote)),
      on_disconnect_callback_(std::move(on_disconnect_callback)) {}

NearbyPresenceService::ScanSession::~ScanSession() {}

}  // namespace ash::nearby::presence
