// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_medium.h"

#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/wifi_direct_server_socket.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestIPv4Address[] = "127.0.0.1";
constexpr int kTestPort = 61234;
constexpr char kTestSSID[] = "DIRECT-xx";
constexpr char kTestPassword[] = "ABCD1234";

class FakeWifiDirectConnection
    : public ash::wifi_direct::mojom::WifiDirectConnection {
 public:
  bool did_associate;
  bool should_associate;
  std::string ipv4_address = kTestIPv4Address;

 private:
  // ash::wifi_direct::mojom::WifiDirectConnection
  void GetProperties(GetPropertiesCallback callback) override {
    auto properties =
        ash::wifi_direct::mojom::WifiDirectConnectionProperties::New();
    properties->ipv4_address = ipv4_address;
    properties->credentials = ash::wifi_direct::mojom::WifiCredentials::New();
    properties->credentials->ssid = kTestSSID;
    properties->credentials->passphrase = kTestPassword;
    std::move(callback).Run(std::move(properties));
  }

  // ash::wifi_direct::mojom::WifiDirectConnection
  void AssociateSocket(::mojo::PlatformHandle handle,
                       AssociateSocketCallback callback) override {
    did_associate = should_associate;
    std::move(callback).Run(should_associate);
  }
};

class FakeWifiDirectManager
    : public ash::wifi_direct::mojom::WifiDirectManager {
 public:
  FakeWifiDirectManager() {}

  ~FakeWifiDirectManager() override {}

  void CreateWifiDirectGroup(
      ash::wifi_direct::mojom::WifiCredentialsPtr credentials,
      CreateWifiDirectGroupCallback callback) override {
    if (!connection_) {
      std::move(callback).Run(
          ash::wifi_direct::mojom::WifiDirectOperationResult::kNotSupported,
          mojo::NullRemote());
    } else {
      mojo::PendingRemote<ash::wifi_direct::mojom::WifiDirectConnection>
          connection_remote;
      mojo::MakeSelfOwnedReceiver(
          std::move(connection_),
          connection_remote.InitWithNewPipeAndPassReceiver());
      std::move(callback).Run(
          ash::wifi_direct::mojom::WifiDirectOperationResult::kSuccess,
          std::move(connection_remote));
    }
  }

  void ConnectToWifiDirectGroup(
      ash::wifi_direct::mojom::WifiCredentialsPtr credentials,
      std::optional<uint32_t> frequency,
      ConnectToWifiDirectGroupCallback callback) override {
    std::move(callback).Run(
        ash::wifi_direct::mojom::WifiDirectOperationResult::kNotSupported,
        mojo::NullRemote());
  }

  void GetWifiP2PCapabilities(
      GetWifiP2PCapabilitiesCallback callback) override {
    // TODO(b/341325756): This is currently ignored, so no need to build a real
    // response.
    auto response = ash::wifi_direct::mojom::WifiP2PCapabilities::New();
    std::move(callback).Run(std::move(response));
  }

  FakeWifiDirectConnection* SetWifiDirectConnection(
      std::unique_ptr<FakeWifiDirectConnection> connection) {
    connection_ = std::move(connection);
    return connection_.get();
  }

 private:
  std::unique_ptr<FakeWifiDirectConnection> connection_;
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
  FakeWifiDirectManager* manager() { return wifi_direct_manager_; }
  ash::nearby::FakeFirewallHoleFactory* firewall_hole_factory() {
    return firewall_hole_factory_;
  }

  void RunOnTaskRunner(base::OnceClosure task) {
    base::RunLoop run_loop;
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTaskAndReply(FROM_HERE, std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

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

// TODO(b/341325756): This test needs to be replaced once a resolution is found
// for the expected response.
TEST_F(WifiDirectMediumTest, IsInterfaceValid_Temporary) {
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(medium->IsInterfaceValid());
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, StartWifiDirect_MissingConnection) {
  manager()->SetWifiDirectConnection(nullptr);
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_FALSE(medium->StartWifiDirect(&credentials));
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, StartWifiDirect_ValidConnection) {
  manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_TRUE(medium->StartWifiDirect(&credentials));
        EXPECT_EQ(credentials.GetIPAddress(), kTestIPv4Address);
        EXPECT_EQ(credentials.GetGateway(), kTestIPv4Address);
        EXPECT_EQ(credentials.GetSSID(), kTestSSID);
        EXPECT_EQ(credentials.GetPassword(), kTestPassword);
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, StopWifiDirect_MissingConnection) {
  manager()->SetWifiDirectConnection(nullptr);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_FALSE(medium->StartWifiDirect(&credentials));
      },
      medium()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->StopWifiDirect());
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, StopWifiDirect_ExistingConnection) {
  manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_TRUE(medium->StartWifiDirect(&credentials));
      },
      medium()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(medium->StopWifiDirect());
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, ListenForService_Success) {
  auto* connection = manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  connection->should_associate = true;
  firewall_hole_factory()->should_succeed_ = true;

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_TRUE(medium->StartWifiDirect(&credentials));
      },
      medium()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(medium->ListenForService(kTestPort));
      },
      medium()));

  EXPECT_TRUE(connection->did_associate);
}

TEST_F(WifiDirectMediumTest, ListenForService_MissingConnection) {
  manager()->SetWifiDirectConnection(nullptr);
  firewall_hole_factory()->should_succeed_ = true;

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_FALSE(medium->StartWifiDirect(&credentials));
      },
      medium()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->ListenForService(kTestPort));
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, ListenForService_FailToAssociatesSocket) {
  auto* connection = manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  connection->should_associate = false;
  firewall_hole_factory()->should_succeed_ = true;

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_TRUE(medium->StartWifiDirect(&credentials));
      },
      medium()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->ListenForService(kTestPort));
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, ListenForService_FailToOpenFirewallHole) {
  auto* connection = manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  connection->should_associate = true;
  firewall_hole_factory()->should_succeed_ = false;

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_TRUE(medium->StartWifiDirect(&credentials));
      },
      medium()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->ListenForService(kTestPort));
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, ListenForService_InvalidAddress) {
  auto* connection = manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  connection->should_associate = true;
  connection->ipv4_address = "nope";
  firewall_hole_factory()->should_succeed_ = true;

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_TRUE(medium->StartWifiDirect(&credentials));
      },
      medium()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->ListenForService(kTestPort));
      },
      medium()));
}

}  // namespace nearby::chrome
