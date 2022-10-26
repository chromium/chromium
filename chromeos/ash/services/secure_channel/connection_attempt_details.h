// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_DETAILS_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_DETAILS_H_

#include <ostream>
#include <string>

#include "chromeos/ash/services/secure_channel/connection_details.h"
#include "chromeos/ash/services/secure_channel/connection_role.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_medium.h"

namespace ash::secure_channel {

// Fields describing a connection attempt. At any given time, at most one
// connection attempt with a given set of ConnectionAttemptDetails should exist.
class ConnectionAttemptDetails {
 public:
  ConnectionAttemptDetails(const DeviceIdPair& device_id_pair,
                           ConnectionMedium connection_medium,
                           ConnectionRole connection_role);
  ConnectionAttemptDetails(const std::string& remote_device_id,
                           const std::string& local_device_id,
                           ConnectionMedium connection_medium,
                           ConnectionRole connection_role);
  ~ConnectionAttemptDetails();

  const std::string& remote_device_id() const {
    return device_id_pair_.remote_device_id();
  }
  const std::string& local_device_id() const {
    return device_id_pair_.local_device_id();
  }
  const DeviceIdPair& device_id_pair() const { return device_id_pair_; }
  ConnectionMedium connection_medium() const { return connection_medium_; }
  ConnectionRole connection_role() const { return connection_role_; }

  // Returns the ConnectionDetails associated with these
  // ConnectionAttemptDetails. Each host device (i.e., Android phone) uses a
  // single device ID for all accounts on the device, so this return value
  // indicates the intrinsic properties of a connection to that device (i.e.,
  // which device it is, and what medium the connection is).
  ConnectionDetails GetAssociatedConnectionDetails() const;

  // Returns whether |connection_details| is associated with these
  // ConnectionAttemptDetails.
  bool CorrespondsToConnectionDetails(
      const ConnectionDetails& connection_details) const;

  bool operator==(const ConnectionAttemptDetails& other) const;
  bool operator!=(const ConnectionAttemptDetails& other) const;
  bool operator<(const ConnectionAttemptDetails& other) const;

 private:
  DeviceIdPair device_id_pair_;
  ConnectionMedium connection_medium_;
  ConnectionRole connection_role_;
};

std::ostream& operator<<(std::ostream& stream,
                         const ConnectionAttemptDetails& details);

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_ATTEMPT_DETAILS_H_
