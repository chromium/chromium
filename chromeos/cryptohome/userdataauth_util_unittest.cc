// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/userdataauth_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace user_data_auth {

TEST(UserDataAuthUtilTest, CryptohomeErrorToMountError) {
  // Test a few commonly seen input output pair.
  EXPECT_EQ(CryptohomeErrorToMountError(CRYPTOHOME_ERROR_NOT_SET),
            cryptohome::MOUNT_ERROR_NONE);
  EXPECT_EQ(CryptohomeErrorToMountError(CRYPTOHOME_ERROR_MOUNT_FATAL),
            cryptohome::MOUNT_ERROR_FATAL);
  EXPECT_EQ(CryptohomeErrorToMountError(CRYPTOHOME_ERROR_TPM_DEFEND_LOCK),
            cryptohome::MOUNT_ERROR_TPM_DEFEND_LOCK);
  EXPECT_EQ(CryptohomeErrorToMountError(CRYPTOHOME_ERROR_TPM_NEEDS_REBOOT),
            cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT);
}

}  // namespace user_data_auth
