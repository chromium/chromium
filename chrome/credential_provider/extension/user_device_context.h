// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_USER_DEVICE_CONTEXT_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_USER_DEVICE_CONTEXT_H_

#include <string>

namespace credential_provider {
namespace extension {

// The user, device and authentication details for task to be able to perform
// its action.
struct UserDeviceContext {
  UserDeviceContext(std::wstring device_resource_id,
                    std::wstring serial_number,
                    std::wstring machine_guid,
                    std::wstring user_sid,
                    std::wstring dm_token);
  UserDeviceContext(const UserDeviceContext& user_device_context);
  ~UserDeviceContext();

  std::wstring device_resource_id;
  std::wstring serial_number;
  std::wstring machine_guid;
  std::wstring user_sid;

  // The dm_token is unique to device. It is uploaded into user device details
  // as part of GCPW login. The agent sends it along with identifiers to
  // authenticate the user.
  std::wstring dm_token;

  bool operator==(const UserDeviceContext& other_user_device_context) const;
};

}  // namespace extension
}  // namespace credential_provider
#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_USER_DEVICE_CONTEXT_H_
