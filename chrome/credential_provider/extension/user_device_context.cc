// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/user_device_context.h"

namespace credential_provider {
namespace extension {

UserDeviceContext::UserDeviceContext(std::wstring device_resource_id,
                                     std::wstring serial_number,
                                     std::wstring machine_guid,
                                     std::wstring user_sid,
                                     std::wstring dm_token)
    : device_resource_id(device_resource_id),
      serial_number(serial_number),
      machine_guid(machine_guid),
      user_sid(user_sid),
      dm_token(dm_token) {}

UserDeviceContext::~UserDeviceContext() {}

UserDeviceContext::UserDeviceContext(const UserDeviceContext& other)
    : device_resource_id(other.device_resource_id),
      serial_number(other.serial_number),
      machine_guid(other.machine_guid),
      user_sid(other.user_sid),
      dm_token(other.dm_token) {}

bool UserDeviceContext::operator==(const UserDeviceContext& other) const {
  return device_resource_id == other.device_resource_id &&
         serial_number == other.serial_number &&
         machine_guid == other.machine_guid && user_sid == other.user_sid &&
         dm_token == other.dm_token;
}

}  // namespace extension
}  // namespace credential_provider
