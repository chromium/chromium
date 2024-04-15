// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_H_

#include "chromeos/ash/components/multidevice/remote_device_ref.h"

namespace ash::tether {

// Represents a Tether host, which can either be a multidevice::RemoteDeviceRef
// (if the host can be connected over Secure Channel), or a PresenceDevice (if
// the host can be connected over Nearby Connections).
struct TetherHost {
 public:
  explicit TetherHost(multidevice::RemoteDeviceRef remote_device_ref);
  TetherHost(const TetherHost&);
  TetherHost& operator=(const TetherHost&) = default;

  ~TetherHost();

  const std::string GetDeviceId() const;
  const std::string& GetName() const;
  const std::string GetTruncatedDeviceIdForLogs() const;

  static std::string TruncateDeviceIdForLogs(const std::string& device_id);

  const std::optional<multidevice::RemoteDeviceRef> remote_device_ref() const {
    return remote_device_ref_;
  }

 private:
  std::optional<multidevice::RemoteDeviceRef> remote_device_ref_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_H_
