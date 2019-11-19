// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/device_id_pair.h"

#include <functional>

#include "base/hash/hash.h"
#include "chromeos/components/multidevice/remote_device_ref.h"

namespace chromeos {

namespace secure_channel {

DeviceIdPair::DeviceIdPair(const std::string& remote_device_id,
                           const std::string& local_device_id)
    : remote_device_id_(remote_device_id), local_device_id_(local_device_id) {}

DeviceIdPair::DeviceIdPair(const DeviceIdPair& other)
    : remote_device_id_(other.remote_device_id_),
      local_device_id_(other.local_device_id_) {}

DeviceIdPair::~DeviceIdPair() = default;

bool DeviceIdPair::operator==(const DeviceIdPair& other) const {
  return remote_device_id_ == other.remote_device_id_ &&
         local_device_id_ == other.local_device_id_;
}

bool DeviceIdPair::operator!=(const DeviceIdPair& other) const {
  return !(*this == other);
}

bool DeviceIdPair::operator<(const DeviceIdPair& other) const {
  if (remote_device_id_ != other.remote_device_id_)
    return remote_device_id_ < other.remote_device_id_;

  return local_device_id_ < other.local_device_id_;
}

size_t DeviceIdPairHash::operator()(const DeviceIdPair& device_id_pair) const {
  static std::hash<std::string> string_hash;
  return base::HashInts64(string_hash(device_id_pair.remote_device_id()),
                          string_hash(device_id_pair.local_device_id()));
}

std::ostream& operator<<(std::ostream& stream,
                         const DeviceIdPair& device_id_pair) {
  stream << "{remote_id: \""
         << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                device_id_pair.remote_device_id())
         << "\", local_id: \""
         << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                device_id_pair.local_device_id())
         << "\"}";
  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
