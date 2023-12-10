// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_TYPES_H_

#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"

namespace cryptohome {

// Type alias that allows to change implementation details of error
// passing without affecting intermediate layers.
using ErrorWrapper = user_data_auth::CryptohomeErrorCode;

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_ERROR_TYPES_H_
