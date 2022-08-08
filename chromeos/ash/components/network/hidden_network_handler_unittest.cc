// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hidden_network_handler.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class HiddenNetworkHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kHiddenNetworkMigration);
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            helper_.network_state_handler(),
            nullptr /* network_device_handler */);
    hidden_network_handler_ = std::make_unique<HiddenNetworkHandler>();
    hidden_network_handler_->Init(helper_.network_state_handler(),
                                  network_configuration_handler_.get());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  NetworkStateTestHelper helper_{false /* use_default_devices_and_services */};
  std::unique_ptr<HiddenNetworkHandler> hidden_network_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
};

TEST_F(HiddenNetworkHandlerTest, CanConstruct) {
  EXPECT_TRUE(true);
}

}  // namespace ash
