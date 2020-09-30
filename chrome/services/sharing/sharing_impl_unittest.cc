// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/mock_webrtc_dependencies.h"
#include "chrome/services/sharing/public/mojom/nearby_decoder.mojom.h"
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

  mojo::Remote<NearbyConnectionsMojom> CreateNearbyConnections(
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
    service()->CreateNearbyConnections(
        std::move(dependencies),
        base::BindLambdaForTesting(
            [&](mojo::PendingRemote<NearbyConnectionsMojom> pending_remote) {
              connections.Bind(std::move(pending_remote));
              run_loop.Quit();
            }));
    // Wait until NearbyConnectionsMojom is connected.
    run_loop.Run();
    return connections;
  }

  mojo::Remote<NearbySharingDecoderMojom> CreateNearbySharingDecoder() {
    mojo::Remote<NearbySharingDecoderMojom> remote;
    base::RunLoop run_loop;
    service()->CreateNearbySharingDecoder(base::BindLambdaForTesting(
        [&](mojo::PendingRemote<NearbySharingDecoderMojom> pending_remote) {
          remote.Bind(std::move(pending_remote));
          run_loop.Quit();
        }));
    // Wait until NearbySharingDecoderMojom is connected.
    run_loop.Run();
    return remote;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::Sharing> remote_;
  std::unique_ptr<SharingImpl> service_;
};

TEST_F(SharingImplTest, NearbyConnections_Create) {
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());

  EXPECT_TRUE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_CreateMultiple) {
  bluetooth::FakeAdapter bluetooth_adapter_1;
  sharing::MockWebRtcDependencies webrtc_dependencies_1;
  mojo::Remote<NearbyConnectionsMojom> connections_1 = CreateNearbyConnections(
      bluetooth_adapter_1.adapter_.BindNewPipeAndPassRemote(),
      webrtc_dependencies_1.socket_manager_.BindNewPipeAndPassRemote(),
      webrtc_dependencies_1.mdns_responder_.BindNewPipeAndPassRemote(),
      webrtc_dependencies_1.ice_config_fetcher_.BindNewPipeAndPassRemote(),
      webrtc_dependencies_1.messenger_.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections_1.is_connected());

  // Calling CreateNearbyConnections() again should disconnect the old instance.
  bluetooth::FakeAdapter bluetooth_adapter_2;
  sharing::MockWebRtcDependencies webrtc_dependencies_2;
  mojo::Remote<NearbyConnectionsMojom> connections_2 = CreateNearbyConnections(
      bluetooth_adapter_2.adapter_.BindNewPipeAndPassRemote(),
      webrtc_dependencies_2.socket_manager_.BindNewPipeAndPassRemote(),
      webrtc_dependencies_2.mdns_responder_.BindNewPipeAndPassRemote(),
      webrtc_dependencies_2.ice_config_fetcher_.BindNewPipeAndPassRemote(),
      webrtc_dependencies_2.messenger_.BindNewPipeAndPassRemote());

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections_1.is_connected());
  EXPECT_TRUE(connections_2.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_BluetoothDisconnects) {
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections.is_connected());

  // Disconnecting the |bluetooth_adapter| interface should also
  // disconnect and destroy the |connections| interface.
  bluetooth_adapter.adapter_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcSignalingMessengerDisconnects) {
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections.is_connected());

  // Disconnecting the |webrtc_dependencies.messenger_| interface should also
  // disconnect and destroy the |connections| interface.
  webrtc_dependencies.messenger_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcMdnsResponderDisconnects) {
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections.is_connected());

  // Disconnecting the |webrtc_dependencies.mdns_responder_| interface should
  // also disconnect and destroy the |connections| interface.
  webrtc_dependencies.mdns_responder_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcP2PSocketManagerDisconnects) {
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections.is_connected());

  // Disconnecting the |webrtc_dependencies.socket_manager_| interface should
  // also disconnect and destroy the |connections| interface.
  webrtc_dependencies.socket_manager_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_WebRtcIceConfigFetcherDisconnects) {
  bluetooth::FakeAdapter bluetooth_adapter;
  sharing::MockWebRtcDependencies webrtc_dependencies;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      bluetooth_adapter.adapter_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections.is_connected());

  // Disconnecting the |webrtc_dependencies.ice_config_fetcher_| interface
  // should also disconnect and destroy the |connections| interface.
  webrtc_dependencies.ice_config_fetcher_.reset();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbyConnections_NullBluetoothAdapter) {
  sharing::MockWebRtcDependencies webrtc_dependencies;
  mojo::Remote<NearbyConnectionsMojom> connections = CreateNearbyConnections(
      mojo::NullRemote(),
      webrtc_dependencies.socket_manager_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.mdns_responder_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.ice_config_fetcher_.BindNewPipeAndPassRemote(),
      webrtc_dependencies.messenger_.BindNewPipeAndPassRemote());
  EXPECT_TRUE(connections.is_connected());
}

TEST_F(SharingImplTest, NearbySharingDecoder_Create) {
  mojo::Remote<NearbySharingDecoderMojom> remote = CreateNearbySharingDecoder();
  EXPECT_TRUE(remote.is_connected());
}

TEST_F(SharingImplTest, NearbySharingDecoder_CreateMultiple) {
  mojo::Remote<NearbySharingDecoderMojom> remote_1 =
      CreateNearbySharingDecoder();
  EXPECT_TRUE(remote_1.is_connected());

  // Calling CreateNearbySharingDecoder() again should disconnect the old
  // instance.
  mojo::Remote<NearbySharingDecoderMojom> remote_2 =
      CreateNearbySharingDecoder();

  // Run mojo disconnect handlers.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(remote_1.is_connected());
  EXPECT_TRUE(remote_2.is_connected());
}

}  // namespace sharing
