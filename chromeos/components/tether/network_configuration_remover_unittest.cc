// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_configuration_remover.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/network/mock_managed_network_configuration_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;

namespace chromeos {

namespace tether {

namespace {

const char kWifiNetworkPath[] = "wifiNetworkPath";

}  // namespace

class NetworkConfigurationRemoverTest : public testing::Test {
 protected:
  NetworkConfigurationRemoverTest() = default;
  ~NetworkConfigurationRemoverTest() override = default;

  void SetUp() override {
    mock_managed_network_configuration_manager_ =
        base::WrapUnique(new NiceMock<MockManagedNetworkConfigurationHandler>);

    network_configuration_remover_ =
        base::WrapUnique(new NetworkConfigurationRemover(
            mock_managed_network_configuration_manager_.get()));
  }

  std::unique_ptr<MockManagedNetworkConfigurationHandler>
      mock_managed_network_configuration_manager_;

  std::unique_ptr<NetworkConfigurationRemover> network_configuration_remover_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationRemoverTest);
};

TEST_F(NetworkConfigurationRemoverTest, TestRemoveNetworkConfiguration) {
  EXPECT_CALL(*mock_managed_network_configuration_manager_,
              RemoveConfiguration(kWifiNetworkPath, _, _))
      .Times(1);

  network_configuration_remover_->RemoveNetworkConfigurationByPath(
      kWifiNetworkPath);
}

}  // namespace tether

}  // namespace chromeos
