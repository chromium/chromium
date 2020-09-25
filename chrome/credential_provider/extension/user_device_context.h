// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_USER_DEVICE_CONTEXT_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_USER_DEVICE_CONTEXT_H_

#include <string>

#include "base/strings/string16.h"

namespace credential_provider {
namespace extension {

// The configuration the task needs to run on. A way to tell task manager on how
// to run the task.
struct Config {};

// The user, device and authentication details for task to be able to perform
// its action.
struct UserDeviceContext {
  UserDeviceContext(base::string16 device_resource_id,
                    base::string16 serial_number,
                    base::string16 machine_guid,
                    base::string16 user_sid,
                    base::string16 dm_token);
  UserDeviceContext(const UserDeviceContext& user_device_context);
  ~UserDeviceContext();

  base::string16 device_resource_id;
  base::string16 serial_number;
  base::string16 machine_guid;
  base::string16 user_sid;

  // The dm_token is unique to device. It is uploaded into user device details
  // as part of GCPW login. The agent sends it along with identifiers to
  // authenticate the user.
  base::string16 dm_token;

  bool operator==(const UserDeviceContext& other_user_device_context);
};

}  // namespace extension
}  // namespace credential_provider
#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_USER_DEVICE_CONTEXT_H_
