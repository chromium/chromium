// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/user_device_context.h"

namespace credential_provider {
namespace extension {

UserDeviceContext::UserDeviceContext(base::string16 serial_number,
                                     base::string16 machine_guid,
                                     base::string16 gaia_profile_id,
                                     base::string16 dm_token)
    : serial_number_(serial_number),
      machine_guid_(machine_guid),
      gaia_profile_id_(gaia_profile_id),
      dm_token_(dm_token) {}

UserDeviceContext::~UserDeviceContext() {}

}  // namespace extension
}  // namespace credential_provider
