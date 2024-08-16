// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/session_manager/policy_descriptor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kEmptyAccountId[] = "";

}  // namespace

using SessionManagerPolicyUtilTest = ::testing::Test;

TEST_F(SessionManagerPolicyUtilTest, MakeChromePolicyDescriptor) {
  login_manager::PolicyDescriptor descriptor = ash::MakeChromePolicyDescriptor(
      login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT, kEmptyAccountId);

  EXPECT_EQ(descriptor.account_type(),
            login_manager::ACCOUNT_TYPE_DEVICE_LOCAL_ACCOUNT);
  EXPECT_EQ(descriptor.account_id(), kEmptyAccountId);
  EXPECT_EQ(descriptor.domain(), login_manager::POLICY_DOMAIN_CHROME);
}

}  // namespace ash
