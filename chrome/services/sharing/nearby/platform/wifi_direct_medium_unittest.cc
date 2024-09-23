// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_medium.h"

#include <netinet/in.h>

#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/wifi_direct_server_socket.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/log/net_log_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestIPv4Address[] = "127.0.0.1";
constexpr uint16_t kMinPort = 50000;  // Arbitrary port.
constexpr uint16_t kMaxPort = 65535;
constexpr char kTestSSID[] = "DIRECT-xx";
constexpr char kTestPassword[] = "ABCD1234";
constexpr int32_t kTestFrequency = 123456;

constexpr char kIsP2pSupportedMetricName[] =
    "Nearby.Connections.WifiDirect.IsP2pSupported";
constexpr char kCreateGroupResultMetricName[] =
    "Nearby.Connections.WifiDirect.CreateWifiDirectGroup.Result";
constexpr char kCreateGroupErrorMetricName[] =
    "Nearby.Connections.WifiDirect.CreateWifiDirectGroup.Error";
constexpr char kConnectGroupResultMetricName[] =
    "Nearby.Connections.WifiDirect.ConnectToWifiDirectGroup.Result";
constexpr char kConnectGroupErrorMetricName[] =
    "Nearby.Connections.WifiDirect.ConnectToWifiDirectGroup.Error";
constexpr char kAssociateSocketResultMetricName[] =
    "Nearby.Connections.WifiDirect.AssociateSocket.Result";
constexpr char kConnectToServiceResultMetricName[] =
    "Nearby.Connections.WifiDirect.ConnectToService.Result";
constexpr char kConnectToServiceErrorMetricName[] =
    "Nearby.Connections.WifiDirect.ConnectToService.Error";
constexpr char kListenForServiceResultMetricName[] =
    "Nearby.Connections.WifiDirect.ListenForService.Result";
constexpr char kListenForServiceErrorMetricName[] =
    "Nearby.Connections.WifiDirect.ListenForService.Error";

// Pick a random port for each test run, otherwise the `Listen` call has a
// chance to return ADDRESS_IN_USE(-147).
int RandomPort() {
  return static_cast<uint16_t>(kMinPort +
                               base::RandGenerator(kMaxPort - kMinPort + 1));
}

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
    properties->frequency = kTestFrequency;
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
    ReturnConnection(std::move(callback));
  }

  void ConnectToWifiDirectGroup(
      ash::wifi_direct::mojom::WifiCredentialsPtr credentials,
      std::optional<uint32_t> frequency,
      ConnectToWifiDirectGroupCallback callback) override {
    EXPECT_EQ(credentials->ssid, expected_ssid_);
    EXPECT_EQ(credentials->passphrase, expected_password_);
    EXPECT_EQ(frequency, expected_frequency_);
    ReturnConnection(std::move(callback));
  }

  void GetWifiP2PCapabilities(
      GetWifiP2PCapabilitiesCallback callback) override {
    auto response = ash::wifi_direct::mojom::WifiP2PCapabilities::New();
    response->is_p2p_supported = is_interface_valid_;
    std::move(callback).Run(std::move(response));
  }

  FakeWifiDirectConnection* SetWifiDirectConnection(
      std::unique_ptr<FakeWifiDirectConnection> connection) {
    connection_ = std::move(connection);
    return connection_.get();
  }

  void SetIsInterfaceValid(bool is_valid) { is_interface_valid_ = is_valid; }

  void SetExpectedCredentials(const std::string& ssid,
                              const std::string& password) {
    expected_ssid_ = ssid;
    expected_password_ = password;
  }

  void SetExpectedFrequency(std::optional<uint32_t> frequency) {
    expected_frequency_ = frequency;
  }

 private:
  void ReturnConnection(ConnectToWifiDirectGroupCallback callback) {
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

  std::string expected_ssid_ = "";
  std::string expected_password_ = "";
  std::optional<uint32_t> expected_frequency_ = std::nullopt;
  std::unique_ptr<FakeWifiDirectConnection> connection_;
  bool is_interface_valid_ = true;
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

  void AcceptSocket(int port) {
    auto ip_endpoint = net::IPEndPoint(
        net::IPAddress::FromIPLiteral(kTestIPv4Address).value(), port);
    server_socket_ =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());

    auto fd = base::ScopedFD(
        net::CreatePlatformSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    CHECK(fd.get() > 0);
    server_socket_->AdoptSocket(fd.release());

    std::optional<net::IPAddress> address =
        net::IPAddress::FromIPLiteral(kTestIPv4Address);
    CHECK(address);
    int listen_result =
        server_socket_->Listen(net::IPEndPoint(*address, port), 4,
                               /*ipv6_only=*/std::nullopt);
    CHECK(listen_result == net::OK);

    int result = server_socket_->Accept(
        &accepted_socket_, base::BindOnce(&WifiDirectMediumTest::OnAccept,
                                          base::Unretained(this)));
    if (result != net::ERR_IO_PENDING) {
      OnAccept(result);
    }
  }

  void OnAccept(int result) {
    EXPECT_EQ(result, net::OK);
    accepted_socket_->Disconnect();
  }

  WifiDirectMedium* medium() { return medium_.get(); }
  FakeWifiDirectManager* manager() { return wifi_direct_manager_; }
  ash::nearby::FakeFirewallHoleFactory* firewall_hole_factory() {
    return firewall_hole_factory_;
  }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void RunOnTaskRunner(base::OnceClosure task) {
    base::RunLoop run_loop;
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTaskAndReply(FROM_HERE, std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  raw_ptr<FakeWifiDirectManager> wifi_direct_manager_;
  mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>
      wifi_direct_manager_remote_;
  raw_ptr<ash::nearby::FakeFirewallHoleFactory> firewall_hole_factory_;
  mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>
      firewall_hole_factory_remote_;
  std::unique_ptr<WifiDirectMedium> medium_;
  std::unique_ptr<net::TCPServerSocket> server_socket_;
  std::unique_ptr<net::StreamSocket> accepted_socket_;
  base::HistogramTester histogram_tester_;
};

TEST_F(WifiDirectMediumTest, IsInterfaceValid_Valid) {
  manager()->SetIsInterfaceValid(true);
  histogram_tester().ExpectTotalCount(kIsP2pSupportedMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(medium->IsInterfaceValid());
      },
      medium()));

  histogram_tester().ExpectTotalCount(kIsP2pSupportedMetricName, 1);
  histogram_tester().ExpectBucketCount(kIsP2pSupportedMetricName,
                                       /*bucket:true=*/1, 1);
}

TEST_F(WifiDirectMediumTest, IsInterfaceValid_Invalid) {
  manager()->SetIsInterfaceValid(false);
  histogram_tester().ExpectTotalCount(kIsP2pSupportedMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->IsInterfaceValid());
      },
      medium()));

  histogram_tester().ExpectTotalCount(kIsP2pSupportedMetricName, 1);
  histogram_tester().ExpectBucketCount(kIsP2pSupportedMetricName,
                                       /*bucket:false=*/0, 1);
}

TEST_F(WifiDirectMediumTest, StartWifiDirect_MissingConnection) {
  manager()->SetWifiDirectConnection(nullptr);
  histogram_tester().ExpectTotalCount(kCreateGroupResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kCreateGroupErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_FALSE(medium->StartWifiDirect(&credentials));
      },
      medium()));

  histogram_tester().ExpectTotalCount(kCreateGroupResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kCreateGroupResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectTotalCount(kCreateGroupErrorMetricName, 1);
  histogram_tester().ExpectBucketCount(
      kCreateGroupErrorMetricName,
      ash::wifi_direct::mojom::WifiDirectOperationResult::kNotSupported, 1);
}

TEST_F(WifiDirectMediumTest, StartWifiDirect_ValidConnection) {
  manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  histogram_tester().ExpectTotalCount(kCreateGroupResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kCreateGroupErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        EXPECT_TRUE(medium->StartWifiDirect(&credentials));
        EXPECT_EQ(credentials.GetIPAddress(), kTestIPv4Address);
        EXPECT_EQ(credentials.GetGateway(), kTestIPv4Address);
        EXPECT_EQ(credentials.GetSSID(), kTestSSID);
        EXPECT_EQ(credentials.GetPassword(), kTestPassword);
        EXPECT_EQ(credentials.GetFrequency(), kTestFrequency);
      },
      medium()));

  histogram_tester().ExpectTotalCount(kCreateGroupResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kCreateGroupResultMetricName,
                                       /*bucket:true=*/1, 1);
  histogram_tester().ExpectTotalCount(kCreateGroupErrorMetricName, 0);
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

TEST_F(WifiDirectMediumTest, ConnectWifiDirect_MissingConnection) {
  manager()->SetWifiDirectConnection(nullptr);
  manager()->SetExpectedCredentials(kTestSSID, kTestPassword);
  manager()->SetExpectedFrequency(kTestFrequency);
  histogram_tester().ExpectTotalCount(kConnectGroupResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kConnectGroupErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        credentials.SetSSID(kTestSSID);
        credentials.SetPassword(kTestPassword);
        credentials.SetFrequency(kTestFrequency);
        EXPECT_FALSE(medium->ConnectWifiDirect(&credentials));
      },
      medium()));

  histogram_tester().ExpectTotalCount(kConnectGroupResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kConnectGroupResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectTotalCount(kConnectGroupErrorMetricName, 1);
  histogram_tester().ExpectBucketCount(
      kConnectGroupErrorMetricName,
      ash::wifi_direct::mojom::WifiDirectOperationResult::kNotSupported, 1);
}

TEST_F(WifiDirectMediumTest, ConnectWifiDirect_ValidConnection_ValidFrequency) {
  manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  manager()->SetExpectedCredentials(kTestSSID, kTestPassword);
  manager()->SetExpectedFrequency(kTestFrequency);
  histogram_tester().ExpectTotalCount(kConnectGroupResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kConnectGroupErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        credentials.SetSSID(kTestSSID);
        credentials.SetPassword(kTestPassword);
        credentials.SetFrequency(kTestFrequency);
        EXPECT_TRUE(medium->ConnectWifiDirect(&credentials));
      },
      medium()));

  histogram_tester().ExpectTotalCount(kConnectGroupResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kConnectGroupResultMetricName,
                                       /*bucket:true=*/1, 1);
  histogram_tester().ExpectTotalCount(kConnectGroupErrorMetricName, 0);
}

TEST_F(WifiDirectMediumTest,
       ConnectWifiDirect_ValidConnection_InvalidFrequency) {
  manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  manager()->SetExpectedCredentials(kTestSSID, kTestPassword);
  manager()->SetExpectedFrequency(std::nullopt);
  histogram_tester().ExpectTotalCount(kConnectGroupResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kConnectGroupErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        credentials.SetSSID(kTestSSID);
        credentials.SetPassword(kTestPassword);
        credentials.SetFrequency(-1);
        EXPECT_TRUE(medium->ConnectWifiDirect(&credentials));
      },
      medium()));

  histogram_tester().ExpectTotalCount(kConnectGroupResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kConnectGroupResultMetricName,
                                       /*bucket:true=*/1, 1);
  histogram_tester().ExpectTotalCount(kConnectGroupErrorMetricName, 0);
}

TEST_F(WifiDirectMediumTest, DisconnectWifiDirect_MissingConnection) {
  manager()->SetWifiDirectConnection(nullptr);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        credentials.SetFrequency(0);
        EXPECT_FALSE(medium->ConnectWifiDirect(&credentials));
      },
      medium()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->DisconnectWifiDirect());
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, DisconnectWifiDirect_ExistingConnection) {
  manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  manager()->SetExpectedCredentials(kTestSSID, kTestPassword);
  manager()->SetExpectedFrequency(kTestFrequency);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        credentials.SetSSID(kTestSSID);
        credentials.SetPassword(kTestPassword);
        credentials.SetFrequency(kTestFrequency);
        EXPECT_TRUE(medium->ConnectWifiDirect(&credentials));
      },
      medium()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(medium->DisconnectWifiDirect());
      },
      medium()));
}

TEST_F(WifiDirectMediumTest, ConnectToService_Success) {
  auto* connection = manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  connection->should_associate = true;
  manager()->SetExpectedCredentials(kTestSSID, kTestPassword);
  manager()->SetExpectedFrequency(kTestFrequency);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        credentials.SetSSID(kTestSSID);
        credentials.SetPassword(kTestPassword);
        credentials.SetFrequency(kTestFrequency);
        EXPECT_TRUE(medium->ConnectWifiDirect(&credentials));
      },
      medium()));

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kConnectToServiceResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kConnectToServiceErrorMetricName, 0);

  int port = RandomPort();
  AcceptSocket(port);
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium, int port) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        CancellationFlag cancellation_flag;
        EXPECT_TRUE(medium->ConnectToService(kTestIPv4Address, port,
                                             &cancellation_flag));
      },
      medium(), port));

  EXPECT_TRUE(connection->did_associate);
  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 1);
  histogram_tester().ExpectTotalCount(kConnectToServiceResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kConnectToServiceResultMetricName,
                                       /*bucket:true=*/1, 1);
  histogram_tester().ExpectTotalCount(kConnectToServiceErrorMetricName, 0);
}

TEST_F(WifiDirectMediumTest, ConnectToService_MissingConnection) {
  manager()->SetWifiDirectConnection(nullptr);
  manager()->SetExpectedCredentials(kTestSSID, kTestPassword);
  manager()->SetExpectedFrequency(kTestFrequency);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        credentials.SetSSID(kTestSSID);
        credentials.SetPassword(kTestPassword);
        credentials.SetFrequency(kTestFrequency);
        EXPECT_FALSE(medium->ConnectWifiDirect(&credentials));
      },
      medium()));

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kConnectToServiceResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kConnectToServiceErrorMetricName, 0);

  int port = RandomPort();
  AcceptSocket(port);
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium, int port) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        CancellationFlag cancellation_flag;
        EXPECT_FALSE(medium->ConnectToService(kTestIPv4Address, port,
                                              &cancellation_flag));
      },
      medium(), port));

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectBucketCount(kConnectToServiceResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectTotalCount(kConnectToServiceErrorMetricName, 1);
  histogram_tester().ExpectBucketCount(kConnectToServiceErrorMetricName,
                                       WifiDirectServiceError::kNoConnection,
                                       1);
}

TEST_F(WifiDirectMediumTest, ConnectToService_FailToAssociatesSocket) {
  auto* connection = manager()->SetWifiDirectConnection(
      std::make_unique<FakeWifiDirectConnection>());
  connection->should_associate = false;
  manager()->SetExpectedCredentials(kTestSSID, kTestPassword);
  manager()->SetExpectedFrequency(kTestFrequency);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        WifiDirectCredentials credentials;
        credentials.SetSSID(kTestSSID);
        credentials.SetPassword(kTestPassword);
        credentials.SetFrequency(kTestFrequency);
        EXPECT_TRUE(medium->ConnectWifiDirect(&credentials));
      },
      medium()));

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kConnectToServiceResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kConnectToServiceErrorMetricName, 0);

  int port = RandomPort();
  AcceptSocket(port);
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium, int port) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        CancellationFlag cancellation_flag;
        EXPECT_FALSE(medium->ConnectToService(kTestIPv4Address, port,
                                              &cancellation_flag));
      },
      medium(), port));

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kAssociateSocketResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectBucketCount(kConnectToServiceResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectTotalCount(kConnectToServiceErrorMetricName, 1);
  histogram_tester().ExpectBucketCount(
      kConnectToServiceErrorMetricName,
      WifiDirectServiceError::kFailedToAssociateSocket, 1);
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

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(medium->ListenForService(RandomPort()));
      },
      medium()));

  EXPECT_TRUE(connection->did_associate);
  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kAssociateSocketResultMetricName,
                                       /*bucket:true=*/1, 1);
  histogram_tester().ExpectTotalCount(kListenForServiceResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kListenForServiceResultMetricName,
                                       /*bucket:true=*/1, 1);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 0);
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

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->ListenForService(RandomPort()));
      },
      medium()));

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectBucketCount(kListenForServiceResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 1);
  histogram_tester().ExpectBucketCount(kListenForServiceErrorMetricName,
                                       WifiDirectServiceError::kNoConnection,
                                       1);
}

TEST_F(WifiDirectMediumTest, ListenForService_FailToAssociateSocket) {
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

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->ListenForService(RandomPort()));
      },
      medium()));

  EXPECT_FALSE(connection->did_associate);
  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kAssociateSocketResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectBucketCount(kListenForServiceResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 1);
  histogram_tester().ExpectBucketCount(
      kListenForServiceErrorMetricName,
      WifiDirectServiceError::kFailedToAssociateSocket, 1);
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

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->ListenForService(RandomPort()));
      },
      medium()));

  EXPECT_TRUE(connection->did_associate);
  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kAssociateSocketResultMetricName,
                                       /*bucket:true=*/1, 1);
  histogram_tester().ExpectBucketCount(kListenForServiceResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 1);
  histogram_tester().ExpectBucketCount(
      kListenForServiceErrorMetricName,
      WifiDirectServiceError::kFailedToOpenFirewallHole, 1);
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

  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectMedium* medium) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(medium->ListenForService(RandomPort()));
      },
      medium()));

  EXPECT_TRUE(connection->did_associate);
  histogram_tester().ExpectTotalCount(kAssociateSocketResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kAssociateSocketResultMetricName,
                                       /*bucket:true=*/1, 1);
  histogram_tester().ExpectBucketCount(kListenForServiceResultMetricName,
                                       /*bucket:false=*/0, 1);
  histogram_tester().ExpectTotalCount(kListenForServiceErrorMetricName, 1);
  histogram_tester().ExpectBucketCount(
      kListenForServiceErrorMetricName,
      WifiDirectServiceError::kFailedToListenToSocket, 1);
}

}  // namespace nearby::chrome
