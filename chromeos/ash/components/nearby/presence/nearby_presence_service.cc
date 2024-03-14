// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"

namespace ash::nearby::presence {

NearbyPresenceService::NearbyPresenceService() = default;
NearbyPresenceService::~NearbyPresenceService() = default;

NearbyPresenceService::ScanFilter::ScanFilter(
    ::nearby::internal::IdentityType identity_type,
    const std::vector<Action>& actions)
    : identity_type_(identity_type), actions_(actions) {}

NearbyPresenceService::ScanFilter::~ScanFilter() = default;

NearbyPresenceService::ScanFilter::ScanFilter(const ScanFilter& scan_filter) {
  identity_type_ = scan_filter.identity_type_;
  actions_ = scan_filter.actions_;
}

NearbyPresenceService::ScanDelegate::ScanDelegate() = default;
NearbyPresenceService::ScanDelegate::~ScanDelegate() = default;

NearbyPresenceService::ScanSession::ScanSession(
    mojo::PendingRemote<ash::nearby::presence::mojom::ScanSession>
        pending_remote,
    base::OnceClosure on_disconnect_callback)
    : remote_(std::move(pending_remote)),
      on_disconnect_callback_(std::move(on_disconnect_callback)) {}

NearbyPresenceService::ScanSession::~ScanSession() {}

}  // namespace ash::nearby::presence
