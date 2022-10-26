// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/connection_details.h"

#include <tuple>

#include "base/check.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"

namespace ash::secure_channel {

ConnectionDetails::ConnectionDetails(const std::string& device_id,
                                     ConnectionMedium connection_medium)
    : device_id_(device_id), connection_medium_(connection_medium) {
  DCHECK(!device_id.empty());
}

ConnectionDetails::~ConnectionDetails() = default;

bool ConnectionDetails::operator==(const ConnectionDetails& other) const {
  return device_id() == other.device_id() &&
         connection_medium() == other.connection_medium();
}

bool ConnectionDetails::operator!=(const ConnectionDetails& other) const {
  return !(*this == other);
}

bool ConnectionDetails::operator<(const ConnectionDetails& other) const {
  return std::tie(device_id_, connection_medium_) <
         std::tie(other.device_id_, other.connection_medium_);
}

std::ostream& operator<<(std::ostream& stream,
                         const ConnectionDetails& details) {
  stream << "{id: \""
         << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                details.device_id())
         << "\", medium: \"" << details.connection_medium() << "\"}";
  return stream;
}

}  // namespace ash::secure_channel
