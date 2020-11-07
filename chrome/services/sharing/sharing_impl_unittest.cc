// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/mock_webrtc_dependencies.h"
#include "chromeos/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

class SharingImplTest : public testing::Test {
 public:
  using NearbyConnectionsMojom =
      location::nearby::connections::mojom::NearbyConnections;
  using NearbySharingDecoderMojom = sharing::mojom::NearbySharingDecoder;
  using NearbyConnections = location::nearby::connections::NearbyConnections;

  SharingImplTest() {
    service_ =
        std::make_unique<SharingImpl>(remote_.BindNewPipeAndPassReceiver(),
                                      /*io_task_runner=*/nullptr);
  }

  ~SharingImplTest() override {
    // Let libjingle threads finish.
    base::RunLoop().RunUntilIdle();
  }

  SharingImpl* service() const { return service_.get(); }

  void Connect(
      mojo::PendingReceiver<NearbyConnectionsMojom> connections_receiver,
      mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder>
          decoder_receiver,
      mojo::PendingRemote<bluetooth::mojom::Adapter> bluetooth_adapter,
      mojo::PendingRemote<network::mojom::P2PSocketManager> socket_manager,
      mojo::PendingRemote<network::mojom::MdnsResponder> mdns_responder,
      mojo::PendingRemote<sharing::mojom::IceConfigFetcher> ice_config_fetcher,
      mojo::PendingRemote<sharing::mojom::WebRtcSignalingMessenger>
          webrtc_signaling_messenger) {
    mojo::Remote<NearbyConnectionsMojom> connections;
    auto webrtc_dependencies =
        location::nearby::connections::mojom::WebRtcDependencies::New(
            std::move(socket_manager), std::move(mdns_responder),
            std::move(ice_config_fetcher),
            std::move(webrtc_signaling_messenger));
    auto dependencies =
        location::nearby::connections::mojom::NearbyConnectionsDependencies::
            New(std::move(bluetooth_adapter), std::move(webrtc_dependencies));
    base::RunLoop run_loop;
    service()->Connect(std::move(dependencies), std::move(connections_receiver),
                       std::move(decoder_receiver));

    // Run Mojo connection handlers.
    base::RunLoop().RunUntilIdle();
  }

  void ShutDown() {
    service()->ShutDown(base::DoNothing());

    // Run Mojo disconnection handlers.
    base::RunLoop().RunUntilIdle();
  }

  bool AreNearbyConnectionsAndDecoderInstancesActive() {
    return service()->nearby_connections_ && service()->nearby_decoder_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::Sharing> remote_;
  std::unique_ptr<SharingImpl> service_;
};

TEST_F(SharingImplTest, ConnectAndShutDown) {
  mojo::Remote<location::nearby::connections::mojom::NearbyConnections>
      connections;
  mojo::Remote<sharing::mojom::NearbySharingDecoder> decoder;
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;

  Connect(connections.BindNewPipeAndPassReceiver(),
          decoder.BindNewPipeAndPassReceiver(),
          bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());

  EXPECT_TRUE(AreNearbyConnectionsAndDecoderInstancesActive());
  EXPECT_TRUE(connections.is_connected());
  EXPECT_TRUE(decoder.is_connected());

  ShutDown();
  EXPECT_FALSE(AreNearbyConnectionsAndDecoderInstancesActive());
  EXPECT_FALSE(connections.is_connected());
  EXPECT_FALSE(decoder.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_BluetoothDisconnects) {
  mojo::Remote<location::nearby::connections::mojom::NearbyConnections>
      connections;
  mojo::Remote<sharing::mojom::NearbySharingDecoder> decoder;
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;

  Connect(connections.BindNewPipeAndPassReceiver(),
          decoder.BindNewPipeAndPassReceiver(),
          bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());

  EXPECT_TRUE(AreNearbyConnectionsAndDecoderInstancesActive());
  EXPECT_TRUE(connections.is_connected());
  EXPECT_TRUE(decoder.is_connected());

  // Disconnecting the |bluetooth_adapter| interface should also
  // disconnect and destroy the |connections| interface.
  bluetooth_adapter.adapter_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcSignalingMessengerDisconnects) {
  mojo::Remote<location::nearby::connections::mojom::NearbyConnections>
      connections;
  mojo::Remote<sharing::mojom::NearbySharingDecoder> decoder;
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;

  Connect(connections.BindNewPipeAndPassReceiver(),
          decoder.BindNewPipeAndPassReceiver(),
          bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());

  EXPECT_TRUE(AreNearbyConnectionsAndDecoderInstancesActive());
  EXPECT_TRUE(connections.is_connected());
  EXPECT_TRUE(decoder.is_connected());

  // Disconnecting the |webrtc_dependencies.messenger_| interface should also
  // disconnect and destroy the |connections| interface.
  webrtc_dependencies.messenger_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcMdnsResponderDisconnects) {
  mojo::Remote<location::nearby::connections::mojom::NearbyConnections>
      connections;
  mojo::Remote<sharing::mojom::NearbySharingDecoder> decoder;
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;

  Connect(connections.BindNewPipeAndPassReceiver(),
          decoder.BindNewPipeAndPassReceiver(),
          bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());

  EXPECT_TRUE(AreNearbyConnectionsAndDecoderInstancesActive());
  EXPECT_TRUE(connections.is_connected());
  EXPECT_TRUE(decoder.is_connected());

  // Disconnecting the |webrtc_dependencies.mdns_responder_| interface should
  // also disconnect and destroy the |connections| interface.
  webrtc_dependencies.mdns_responder_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcP2PSocketManagerDisconnects) {
  mojo::Remote<location::nearby::connections::mojom::NearbyConnections>
      connections;
  mojo::Remote<sharing::mojom::NearbySharingDecoder> decoder;
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;

  Connect(connections.BindNewPipeAndPassReceiver(),
          decoder.BindNewPipeAndPassReceiver(),
          bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());

  EXPECT_TRUE(AreNearbyConnectionsAndDecoderInstancesActive());
  EXPECT_TRUE(connections.is_connected());
  EXPECT_TRUE(decoder.is_connected());

  // Disconnecting the |webrtc_dependencies.socket_manager_| interface should
  // also disconnect and destroy the |connections| interface.
  webrtc_dependencies.socket_manager_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcIceConfigFetcherDisconnects) {
  mojo::Remote<location::nearby::connections::mojom::NearbyConnections>
      connections;
  mojo::Remote<sharing::mojom::NearbySharingDecoder> decoder;
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;

  Connect(connections.BindNewPipeAndPassReceiver(),
          decoder.BindNewPipeAndPassReceiver(),
          bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
          webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());

  EXPECT_TRUE(AreNearbyConnectionsAndDecoderInstancesActive());
  EXPECT_TRUE(connections.is_connected());
  EXPECT_TRUE(decoder.is_connected());

  // Disconnecting the |webrtc_dependencies.ice_config_fetcher_| interface
  // should also disconnect and destroy the |connections| interface.
  webrtc_dependencies.ice_config_fetcher_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

}  // namespace sharing
