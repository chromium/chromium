// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/connection_attempt_details.h"

#include "base/check.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"

namespace ash::secure_channel {

ConnectionAttemptDetails::ConnectionAttemptDetails(
    const DeviceIdPair& device_id_pair,
    ConnectionMedium connection_medium,
    ConnectionRole connection_role)
    : device_id_pair_(device_id_pair),
      connection_medium_(connection_medium),
      connection_role_(connection_role) {}

ConnectionAttemptDetails::ConnectionAttemptDetails(
    const std::string& remote_device_id,
    const std::string& local_device_id,
    ConnectionMedium connection_medium,
    ConnectionRole connection_role)
    : ConnectionAttemptDetails(DeviceIdPair(remote_device_id, local_device_id),
                               connection_medium,
                               connection_role) {}

ConnectionAttemptDetails::~ConnectionAttemptDetails() = default;

ConnectionDetails ConnectionAttemptDetails::GetAssociatedConnectionDetails()
    const {
  return ConnectionDetails(remote_device_id(), connection_medium());
}

bool ConnectionAttemptDetails::CorrespondsToConnectionDetails(
    const ConnectionDetails& connection_details) const {
  return connection_details == GetAssociatedConnectionDetails();
}

bool ConnectionAttemptDetails::operator==(
    const ConnectionAttemptDetails& other) const {
  return device_id_pair() == other.device_id_pair() &&
         connection_medium() == other.connection_medium() &&
         connection_role() == other.connection_role();
}

bool ConnectionAttemptDetails::operator!=(
    const ConnectionAttemptDetails& other) const {
  return !(*this == other);
}

bool ConnectionAttemptDetails::operator<(
    const ConnectionAttemptDetails& other) const {
  if (device_id_pair() != other.device_id_pair())
    return device_id_pair() < other.device_id_pair();

  // Arbitrarily choose the listener role as less than the initiator role.
  if (connection_role() != other.connection_role())
    return connection_role() == ConnectionRole::kListenerRole;

  // Currently, there is only one ConnectionMedium type.
  DCHECK(connection_medium() == other.connection_medium());
  return false;
}

std::ostream& operator<<(std::ostream& stream,
                         const ConnectionAttemptDetails& details) {
  stream << "{remote_device_id: \""
         << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                details.remote_device_id())
         << "\", local_device_id: \""
         << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                details.local_device_id())
         << "\", connection_role: \"" << details.connection_role() << "\", "
         << "connection_medium: \"" << details.connection_medium() << "\"}";
  return stream;
}

}  // namespace ash::secure_channel
