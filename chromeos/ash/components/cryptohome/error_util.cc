// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/error_util.h"

#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"

namespace cryptohome {

bool HasError(ErrorWrapper error) {
  return error.code() != ::user_data_auth::CRYPTOHOME_ERROR_NOT_SET;
}

bool ErrorMatches(ErrorWrapper value,
                  ::user_data_auth::CryptohomeErrorCode error_code) {
  return value.code() == error_code;
}

}  // namespace cryptohome
