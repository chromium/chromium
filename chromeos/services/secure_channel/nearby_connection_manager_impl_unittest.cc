// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/nearby_connection_manager_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelNearbyConnectionManagerImplTest : public testing::Test {
 protected:
  SecureChannelNearbyConnectionManagerImplTest() = default;
  SecureChannelNearbyConnectionManagerImplTest(
      const SecureChannelNearbyConnectionManagerImplTest&) = delete;
  SecureChannelNearbyConnectionManagerImplTest& operator=(
      const SecureChannelNearbyConnectionManagerImplTest&) = delete;
  ~SecureChannelNearbyConnectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    manager_ = NearbyConnectionManagerImpl::Factory::Create();
  }

  std::unique_ptr<NearbyConnectionManager> manager_;
};

// TODO(https://crbug.com/1106937): Delete when a real test is added.
TEST_F(SecureChannelNearbyConnectionManagerImplTest, Create) {
  EXPECT_TRUE(manager_);
}

}  // namespace secure_channel

}  // namespace chromeos
