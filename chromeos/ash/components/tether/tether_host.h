// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_H_

#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "third_party/nearby/src/presence/presence_device.h"

namespace ash::tether {

// Represents a Tether host, which can either be a
// multidevice::RemoteDeviceRef (if the host can be connected over Secure
// Channel), or a PresenceDevice (if the host can be connected over Nearby
// Connections).
struct TetherHost {
 public:
  explicit TetherHost(multidevice::RemoteDeviceRef remote_device_ref);
  explicit TetherHost(const nearby::presence::PresenceDevice& presence_device);
  TetherHost(const TetherHost&);
  TetherHost& operator=(const TetherHost&) = delete;
  ~TetherHost();

  friend bool operator==(const TetherHost& first, const TetherHost& second);

  const std::string GetDeviceId() const;
  const std::string GetName() const;
  const std::string GetTruncatedDeviceIdForLogs() const;

  static std::string TruncateDeviceIdForLogs(const std::string& device_id);

  const std::optional<multidevice::RemoteDeviceRef> remote_device_ref() const {
    return remote_device_ref_;
  }

  const std::optional<nearby::presence::PresenceDevice> presence_device()
      const {
    return presence_device_;
  }

 private:
  std::optional<multidevice::RemoteDeviceRef> remote_device_ref_;
  std::optional<nearby::presence::PresenceDevice> presence_device_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_H_
