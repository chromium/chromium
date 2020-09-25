// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/user_device_context.h"

namespace credential_provider {
namespace extension {

UserDeviceContext::UserDeviceContext(base::string16 device_resource_id,
                                     base::string16 serial_number,
                                     base::string16 machine_guid,
                                     base::string16 user_sid,
                                     base::string16 dm_token)
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

bool UserDeviceContext::operator==(const UserDeviceContext& other) {
  return device_resource_id == other.device_resource_id &&
         serial_number == other.serial_number &&
         machine_guid == other.machine_guid && user_sid == other.user_sid &&
         dm_token == other.dm_token;
}

}  // namespace extension
}  // namespace credential_provider
