// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_ID_PAIR_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_ID_PAIR_H_

#include <ostream>
#include <string>
#include <unordered_set>

namespace ash::secure_channel {

// Pair of IDs belonging to two devices associated with a connection attempt:
// one for the remote device (i.e., the one to which this device is connecting),
// and one for the local device (i.e., the Chromebook).
class DeviceIdPair {
 public:
  DeviceIdPair(const std::string& remote_device_id,
               const std::string& local_device_id);
  DeviceIdPair(const DeviceIdPair& other);
  virtual ~DeviceIdPair();

  const std::string& remote_device_id() const { return remote_device_id_; }
  const std::string& local_device_id() const { return local_device_id_; }

  bool operator==(const DeviceIdPair& other) const;
  bool operator!=(const DeviceIdPair& other) const;
  bool operator<(const DeviceIdPair& other) const;

 private:
  std::string remote_device_id_;
  std::string local_device_id_;
};

struct DeviceIdPairHash {
  size_t operator()(const DeviceIdPair& device_id_pair) const;
};

typedef std::unordered_set<DeviceIdPair, DeviceIdPairHash> DeviceIdPairSet;

std::ostream& operator<<(std::ostream& stream, const DeviceIdPair& details);

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_DEVICE_ID_PAIR_H_
