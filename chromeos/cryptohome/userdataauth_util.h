// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_USERDATAAUTH_UTIL_H_
#define CHROMEOS_CRYPTOHOME_USERDATAAUTH_UTIL_H_

#include "base/component_export.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace user_data_auth {

// Converts user_data_auth::CryptohomeErrorCode to cryptohome::MountError.
COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME)
cryptohome::MountError CryptohomeErrorToMountError(CryptohomeErrorCode code);

}  // namespace user_data_auth

#endif  // CHROMEOS_CRYPTOHOME_USERDATAAUTH_UTIL_H_
