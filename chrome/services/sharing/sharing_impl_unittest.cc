// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chrome/services/sharing/nearby/nearby_presence.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/fake_nearby_presence_credential_storage.h"
#include "chrome/services/sharing/nearby/test_support/mock_webrtc_dependencies.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole_factory.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_mdns_manager.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_socket_factory.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/mdns.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence_credential_storage.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Noop implementation for tests.
class FakeWifiDirectManager
    : public ash::wifi_direct::mojom::WifiDirectManager {
  // ash::wifi_direct::mojom::WifiDirectManager
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

namespace sharing {

class SharingImplTest : public testing::Test {
 public:
  ~SharingImplTest() override {
    // Let libjingle threads finish.
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    service_ =
        std::make_unique<SharingImpl>(remote_.BindNewPipeAndPassReceiver(),
                                      /*io_task_runner=*/nullptr);

    // Set up CrosNetworkConfig mojo service.
    cros_network_config_test_helper_ =
        std::make_unique<ash::network_config::CrosNetworkConfigTestHelper>();
    mojo::PendingRemote<chromeos::network_config::mojom::CrosNetworkConfig>
        cros_network_config_remote;
    ash::GetNetworkConfigService(
        cros_network_config_remote.InitWithNewPipeAndPassReceiver());

    // Set up firewall hole factory mojo service.
    mojo::PendingRemote<::sharing::mojom::FirewallHoleFactory>
        firewall_hole_factory_remote;
    firewall_hole_factory_self_owned_receiver_ref_ =
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<ash::nearby::FakeFirewallHoleFactory>(),
            firewall_hole_factory_remote.InitWithNewPipeAndPassReceiver());

    // Set up TCP socket factory mojo service.
    mojo::PendingRemote<::sharing::mojom::TcpSocketFactory>
        tcp_socket_factory_remote;
    tcp_socket_factory_self_owned_receiver_ref_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<ash::nearby::FakeTcpSocketFactory>(
            /*default_local_addr=*/net::IPEndPoint(
                net::IPAddress(192, 168, 86, 75), 44444)),
        tcp_socket_factory_remote.InitWithNewPipeAndPassReceiver());

    // Set up Mdns manager mojo service.
    mojo::PendingRemote<::sharing::mojom::MdnsManager> mdns_manager_remote;
    mdns_manager_self_owned_receiver_ref_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<ash::nearby::FakeMdnsManager>(),
        mdns_manager_remote.InitWithNewPipeAndPassReceiver());

    // Set up fake WiFiDirect mojo services.
    mojo::PendingRemote<ash::wifi_direct::mojom::WifiDirectManager>
        wifi_direct_manager_remote;
    wifi_direct_manager_self_owned_receiver_ref_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<FakeWifiDirectManager>(),
        wifi_direct_manager_remote.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<::sharing::mojom::FirewallHoleFactory>
        wifi_direct_firewall_hole_factory_remote;
    wifi_direct_firewall_hole_factory_self_owned_receiver_ref_ =
        mojo::MakeSelfOwnedReceiver(
            std::make_unique<ash::nearby::FakeFirewallHoleFactory>(),
            wifi_direct_firewall_hole_factory_remote
                .InitWithNewPipeAndPassReceiver());

    Connect(
        connections_.BindNewPipeAndPassReceiver(),
        presence_.BindNewPipeAndPassReceiver(),
        decoder_.BindNewPipeAndPassReceiver(),
        quick_start_decoder_.BindNewPipeAndPassReceiver(),
        bluetooth_adapter_.adapter_.BindNewPipeAndPassRemote(),
        nearby_presence_credential_storage_.receiver()
            .BindNewPipeAndPassRemote(),
        webrtc_dependencies_.socket_manager_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.mdns_responder_factory_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.ice_config_fetcher_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.messenger_.BindNewPipeAndPassRemote(),
        std::move(cros_network_config_remote),
        std::move(firewall_hole_factory_remote),
        std::move(tcp_socket_factory_remote), std::move(mdns_manager_remote),
        std::move(wifi_direct_manager_remote),
        std::move(wifi_direct_firewall_hole_factory_remote));

    ASSERT_TRUE(AreNearbyInstancesActive());
    ASSERT_TRUE(connections_.is_connected());
    ASSERT_TRUE(presence_.is_connected());
    ASSERT_TRUE(decoder_.is_connected());
    ASSERT_TRUE(quick_start_decoder_.is_connected());
  }

  void Connect(
      mojo::PendingReceiver<nearby::connections::mojom::NearbyConnections>
          connections_receiver,
      mojo::PendingReceiver<ash::nearby::presence::mojom::NearbyPresence>
          presence_receiver,
      mojo::PendingReceiver<::sharing::mojom::NearbySharingDecoder>
          decoder_receiver,
      mojo::PendingReceiver<ash::quick_start::mojom::QuickStartDecoder>
          quick_start_decoder_receiver,
      mojo::PendingRemote<bluetooth::mojom::Adapter> bluetooth_adapter,
      mojo::PendingRemote<
          ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
          nearby_presence_credential_storage,
      mojo::PendingRemote<network::mojom::P2PSocketManager> socket_manager,
      mojo::PendingRemote<::sharing::mojom::MdnsResponderFactory>
          mdns_responder_factory,
      mojo::PendingRemote<::sharing::mojom::IceConfigFetcher>
          ice_config_fetcher,
      mojo::PendingRemote<::sharing::mojom::WebRtcSignalingMessenger>
          webrtc_signaling_messenger,
      mojo::PendingRemote<chromeos::network_config::mojom::CrosNetworkConfig>
          cros_network_config,
      mojo::PendingRemote<::sharing::mojom::FirewallHoleFactory>
          firewall_hole_factory,
      mojo::PendingRemote<::sharing::mojom::TcpSocketFactory>
          tcp_socket_factory,
      mojo::PendingRemote<::sharing::mojom::MdnsManager> mdns_manager,
      mojo::PendingRemote<ash::wifi_direct::mojom::WifiDirectManager>
          wifi_direct_manager,
      mojo::PendingRemote<::sharing::mojom::FirewallHoleFactory>
          wifi_direct_firewall_hole_factory) {
    auto webrtc_dependencies = ::sharing::mojom::WebRtcDependencies::New(
        std::move(socket_manager), std::move(mdns_responder_factory),
        std::move(ice_config_fetcher), std::move(webrtc_signaling_messenger));
    auto wifilan_dependencies = ::sharing::mojom::WifiLanDependencies::New(
        std::move(cros_network_config), std::move(firewall_hole_factory),
        std::move(tcp_socket_factory), std::move(mdns_manager));
    auto wifidirect_dependencies =
        ::sharing::mojom::WifiDirectDependencies::New(
            std::move(wifi_direct_manager),
            std::move(wifi_direct_firewall_hole_factory));
    auto dependencies = ::sharing::mojom::NearbyDependencies::New(
        std::move(bluetooth_adapter), std::move(webrtc_dependencies),
        std::move(wifilan_dependencies), std::move(wifidirect_dependencies),
        std::move(nearby_presence_credential_storage),
        nearby::api::LogMessage::Severity::kInfo);
    base::RunLoop run_loop;
    service_->Connect(std::move(dependencies), std::move(connections_receiver),
                      std::move(presence_receiver), std::move(decoder_receiver),
                      std::move(quick_start_decoder_receiver));

    // Run Mojo connection handlers.
    base::RunLoop().RunUntilIdle();
  }

  void ShutDown() {
    service_->ShutDown(base::DoNothing());

    // Run Mojo disconnection handlers.
    base::RunLoop().RunUntilIdle();
  }

  bool AreNearbyInstancesActive() {
    return service_->nearby_connections_ && service_->nearby_presence_ &&
           service_->nearby_decoder_ && service_->quick_start_decoder_;
  }

  void EnsureDependenciesAreDisconnected() {
    // Run mojo disconnect handlers.
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(AreNearbyInstancesActive());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::Sharing> remote_;
  std::unique_ptr<SharingImpl> service_;

  mojo::Remote<nearby::connections::mojom::NearbyConnections> connections_;
  mojo::Remote<ash::nearby::presence::mojom::NearbyPresence> presence_;
  mojo::Remote<::sharing::mojom::NearbySharingDecoder> decoder_;
  mojo::Remote<ash::quick_start::mojom::QuickStartDecoder> quick_start_decoder_;
  bluetooth::FakeAdapter bluetooth_adapter_;
  ash::nearby::presence::FakeNearbyPresenceCredentialStorage
      nearby_presence_credential_storage_;
  sharing::MockWebRtcDependencies webrtc_dependencies_;
  std::unique_ptr<ash::network_config::CrosNetworkConfigTestHelper>
      cros_network_config_test_helper_;
  mojo::SelfOwnedReceiverRef<::sharing::mojom::FirewallHoleFactory>
      firewall_hole_factory_self_owned_receiver_ref_;
  mojo::SelfOwnedReceiverRef<::sharing::mojom::TcpSocketFactory>
      tcp_socket_factory_self_owned_receiver_ref_;
  mojo::SelfOwnedReceiverRef<::sharing::mojom::MdnsManager>
      mdns_manager_self_owned_receiver_ref_;
  mojo::SelfOwnedReceiverRef<ash::wifi_direct::mojom::WifiDirectManager>
      wifi_direct_manager_self_owned_receiver_ref_;
  mojo::SelfOwnedReceiverRef<::sharing::mojom::FirewallHoleFactory>
      wifi_direct_firewall_hole_factory_self_owned_receiver_ref_;
};

TEST_F(SharingImplTest, ConnectAndShutDown) {
  ShutDown();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_BluetoothDisconnects) {
  bluetooth_adapter_.adapter_.reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_CredentialStorageDisconnects) {
  nearby_presence_credential_storage_.receiver().reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcSignalingMessengerDisconnects) {
  webrtc_dependencies_.messenger_.reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest,
       NearbyConnections_WebRtcMdnsResponderFactoryDisconnects) {
  webrtc_dependencies_.mdns_responder_factory_.reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcP2PSocketManagerDisconnects) {
  webrtc_dependencies_.socket_manager_.reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcIceConfigFetcherDisconnects) {
  webrtc_dependencies_.ice_config_fetcher_.reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_CrosNetworkConfigDisconnects) {
  cros_network_config_test_helper_.reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_FirewallHoleFactoryDisconnects) {
  firewall_hole_factory_self_owned_receiver_ref_->Close();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_TcpSocketFactoryDisconnects) {
  tcp_socket_factory_self_owned_receiver_ref_->Close();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_MdnsManagerDisconnects) {
  mdns_manager_self_owned_receiver_ref_->Close();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_WifiDirectManagerDisconnects) {
  wifi_direct_manager_self_owned_receiver_ref_->Close();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest,
       NearbyConnections_WifiDirectFirewallHoleFactoryDisconnects) {
  wifi_direct_firewall_hole_factory_self_owned_receiver_ref_->Close();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_ConnectionsReset) {
  connections_.reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_PresenceReset) {
  presence_.reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_DecoderReset) {
  decoder_.reset();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_QuickStartDecoderReset) {
  quick_start_decoder_.reset();
  EnsureDependenciesAreDisconnected();
}

}  // namespace sharing
