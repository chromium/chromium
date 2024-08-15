// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host.h"

#include "base/notreached.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"

namespace ash::tether {

TetherHost::TetherHost(const multidevice::RemoteDeviceRef remote_device_ref)
    : remote_device_ref_(remote_device_ref) {}

TetherHost::TetherHost(const nearby::presence::PresenceDevice& presence_device)
    : presence_device_(presence_device) {}

TetherHost::TetherHost(const TetherHost&) = default;

TetherHost::~TetherHost() = default;

bool operator==(const TetherHost& first, const TetherHost& second) {
  if (first.remote_device_ref().has_value()) {
    return second.remote_device_ref().has_value() &&
           first.remote_device_ref().value() ==
               second.remote_device_ref().value();
  }

  if (first.presence_device().has_value()) {
    return second.presence_device().has_value() &&
           first.presence_device().value() == second.presence_device().value();
  }

  NOTREACHED();
}

const std::string TetherHost::GetDeviceId() const {
  if (remote_device_ref_.has_value()) {
    return remote_device_ref_->GetDeviceId();
  }

  if (presence_device_.has_value()) {
    return presence_device_->GetDeviceIdentityMetadata().device_id();
  }

  NOTREACHED();
}

const std::string TetherHost::GetName() const {
  if (presence_device_.has_value()) {
    return presence_device_->GetDeviceIdentityMetadata().device_name();
  }

  if (remote_device_ref_.has_value()) {
    return remote_device_ref_->name();
  }

  NOTREACHED();
}

const std::string TetherHost::GetTruncatedDeviceIdForLogs() const {
  return TruncateDeviceIdForLogs(GetDeviceId());
}

// static:
std::string TetherHost::TruncateDeviceIdForLogs(const std::string& device_id) {
  return multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(device_id);
}

}  // namespace ash::tether
