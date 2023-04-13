// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/proto_conversions.h"

namespace ash::nearby::presence {

::nearby::internal::Metadata BuildMetadata(
    ::nearby::internal::DeviceType device_type,
    const std::string& account_name,
    const std::string& device_name,
    const std::string& user_name,
    const std::string& profile_url,
    const std::string& mac_address) {
  ::nearby::internal::Metadata proto;
  proto.set_device_type(device_type);
  proto.set_account_name(account_name);
  proto.set_user_name(user_name);
  proto.set_device_name(device_name);
  proto.set_user_name(user_name);
  proto.set_device_profile_url(profile_url);
  proto.set_bluetooth_mac_address(mac_address);
  return proto;
}

}  // namespace ash::nearby::presence
