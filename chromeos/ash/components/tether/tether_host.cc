// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host.h"

#include "base/notreached.h"

namespace ash::tether {

TetherHost::TetherHost(const multidevice::RemoteDeviceRef remote_device_ref)
    : remote_device_ref_(remote_device_ref) {}

TetherHost::TetherHost(const TetherHost&) = default;

TetherHost::~TetherHost() = default;

const std::string TetherHost::GetDeviceId() const {
  if (remote_device_ref_.has_value()) {
    return remote_device_ref_->GetDeviceId();
  }

  NOTREACHED_NORETURN();
}

const std::string& TetherHost::GetName() const {
  if (remote_device_ref_.has_value()) {
    return remote_device_ref_->name();
  }

  NOTREACHED_NORETURN();
}

const std::string TetherHost::GetTruncatedDeviceIdForLogs() const {
  if (remote_device_ref_.has_value()) {
    return remote_device_ref_->GetTruncatedDeviceIdForLogs();
  }

  NOTREACHED_NORETURN();
}

// static:
std::string TetherHost::TruncateDeviceIdForLogs(const std::string& device_id) {
  return multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(device_id);
}

}  // namespace ash::tether
