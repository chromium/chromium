// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include <utility>

#include "base/callback_helpers.h"
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
  ~SharingImplTest() override {
    // Let libjingle threads finish.
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    service_ =
        std::make_unique<SharingImpl>(remote_.BindNewPipeAndPassReceiver(),
                                      /*io_task_runner=*/nullptr);

    Connect(
        connections_.BindNewPipeAndPassReceiver(),
        decoder_.BindNewPipeAndPassReceiver(),
        bluetooth_adapter_.adapter_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.socket_manager_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.mdns_responder_factory_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.ice_config_fetcher_.BindNewPipeAndPassRemote(),
        webrtc_dependencies_.messenger_.BindNewPipeAndPassRemote());

    ASSERT_TRUE(AreNearbyConnectionsAndDecoderInstancesActive());
    ASSERT_TRUE(connections_.is_connected());
    ASSERT_TRUE(decoder_.is_connected());
  }

  void Connect(
      mojo::PendingReceiver<
          location::nearby::connections::mojom::NearbyConnections>
          connections_receiver,
      mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder>
          decoder_receiver,
      mojo::PendingRemote<bluetooth::mojom::Adapter> bluetooth_adapter,
      mojo::PendingRemote<network::mojom::P2PSocketManager> socket_manager,
      mojo::PendingRemote<
          location::nearby::connections::mojom::MdnsResponderFactory>
          mdns_responder_factory,
      mojo::PendingRemote<sharing::mojom::IceConfigFetcher> ice_config_fetcher,
      mojo::PendingRemote<sharing::mojom::WebRtcSignalingMessenger>
          webrtc_signaling_messenger) {
    auto webrtc_dependencies =
        location::nearby::connections::mojom::WebRtcDependencies::New(
            std::move(socket_manager), std::move(mdns_responder_factory),
            std::move(ice_config_fetcher),
            std::move(webrtc_signaling_messenger));
    auto dependencies =
        location::nearby::connections::mojom::NearbyConnectionsDependencies::
            New(std::move(bluetooth_adapter), std::move(webrtc_dependencies),
                location::nearby::api::LogMessage::Severity::kInfo);
    base::RunLoop run_loop;
    service_->Connect(std::move(dependencies), std::move(connections_receiver),
                      std::move(decoder_receiver));

    // Run Mojo connection handlers.
    base::RunLoop().RunUntilIdle();
  }

  void ShutDown() {
    service_->ShutDown(base::DoNothing());

    // Run Mojo disconnection handlers.
    base::RunLoop().RunUntilIdle();
  }

  bool AreNearbyConnectionsAndDecoderInstancesActive() {
    return service_->nearby_connections_ && service_->nearby_decoder_;
  }

  void EnsureDependenciesAreDisconnected() {
    // Run mojo disconnect handlers.
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(AreNearbyConnectionsAndDecoderInstancesActive());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::Sharing> remote_;
  std::unique_ptr<SharingImpl> service_;

  mojo::Remote<location::nearby::connections::mojom::NearbyConnections>
      connections_;
  mojo::Remote<sharing::mojom::NearbySharingDecoder> decoder_;
  bluetooth::FakeAdapter bluetooth_adapter_;
  sharing::MockWebRtcDependencies webrtc_dependencies_;
};

TEST_F(SharingImplTest, ConnectAndShutDown) {
  ShutDown();
  EnsureDependenciesAreDisconnected();
}

TEST_F(SharingImplTest, NearbyConnections_BluetoothDisconnects) {
  bluetooth_adapter_.adapter_.reset();
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

}  // namespace sharing
