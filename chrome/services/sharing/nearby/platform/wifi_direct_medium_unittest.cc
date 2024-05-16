// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_medium.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeWifiDirectManager
    : public ash::wifi_direct::mojom::WifiDirectManager {
 public:
  void CreateWifiDirectGroup(
      ash::wifi_direct::mojom::WifiCredentialsPtr credentials,
      CreateWifiDirectGroupCallback callback) override {
    // Noop
  }
  void ConnectToWifiDirectGroup(
      ash::wifi_direct::mojom::WifiCredentialsPtr credentials,
      std::optional<uint32_t> frequency,
      ConnectToWifiDirectGroupCallback callback) override {
    // Noop
  }

  void GetWifiP2PCapabilities(
      GetWifiP2PCapabilitiesCallback callback) override {
    // Noop
  }
};

}  // namespace

namespace nearby::chrome {

class WifiDirectMediumTest : public ::testing::Test {
 public:
  // ::testing::Test
  void SetUp() override {
    // Set up WiFi Direct mojo service.
    auto wifi_direct_manager = std::make_unique<FakeWifiDirectManager>();
    wifi_direct_manager_ = wifi_direct_manager.get();
    mojo::MakeSelfOwnedReceiver(
        std::move(wifi_direct_manager),
        wifi_direct_manager_remote_.BindNewPipeAndPassReceiver());

    // Set up firewall hole factory mojo service.
    auto firewall_hole_factory =
        std::make_unique<ash::nearby::FakeFirewallHoleFactory>();
    firewall_hole_factory_ = firewall_hole_factory.get();
    mojo::MakeSelfOwnedReceiver(
        std::move(firewall_hole_factory),
        firewall_hole_factory_remote_.BindNewPipeAndPassReceiver());

    // Create the subject under test.
    medium_ = std::make_unique<WifiDirectMedium>(wifi_direct_manager_remote_,
                                                 firewall_hole_factory_remote_);
  }

  WifiDirectMedium* medium() { return medium_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  raw_ptr<FakeWifiDirectManager> wifi_direct_manager_;
  mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>
      wifi_direct_manager_remote_;
  raw_ptr<ash::nearby::FakeFirewallHoleFactory> firewall_hole_factory_;
  mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>
      firewall_hole_factory_remote_;
  std::unique_ptr<WifiDirectMedium> medium_;
};

TEST_F(WifiDirectMediumTest, ItBuilds) {
  EXPECT_TRUE(medium());
}

}  // namespace nearby::chrome
