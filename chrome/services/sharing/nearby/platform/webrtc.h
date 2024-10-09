// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WEBRTC_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WEBRTC_H_

#include "third_party/nearby/src/internal/platform/implementation/webrtc.h"

#include <memory>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc_signaling_messenger.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace sharing {
class IpcPacketSocketFactory;
}  // namespace sharing

namespace nearby::chrome {

class WebRtcMedium : public api::WebRtcMedium {
 public:
  using PeerConnectionCallback = api::WebRtcMedium::PeerConnectionCallback;

  WebRtcMedium(
      const mojo::SharedRemote<network::mojom::P2PSocketManager>&
          socket_manager,
      const mojo::SharedRemote<::sharing::mojom::MdnsResponderFactory>&
          mdns_responder_factory,
      const mojo::SharedRemote<::sharing::mojom::IceConfigFetcher>&
          ice_config_fetcher,
      const mojo::SharedRemote<::sharing::mojom::WebRtcSignalingMessenger>&
          webrtc_signaling_messenger,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebRtcMedium() override;

  // api::WebRtcMedium:
  const std::string GetDefaultCountryCode() override;
  void CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                            PeerConnectionCallback callback) override;
  std::unique_ptr<api::WebRtcSignalingMessenger> GetSignalingMessenger(
      std::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint) override;

 private:
  void FetchIceServers(webrtc::PeerConnectionObserver* observer,
                       PeerConnectionCallback callback);
  void OnIceServersFetched(
      webrtc::PeerConnectionObserver* observer,
      PeerConnectionCallback callback,
      std::vector<::sharing::mojom::IceServerPtr> ice_servers)
      LOCKS_EXCLUDED(peer_connection_factory_lock_);

  void InitWebRTCThread(rtc::Thread** thread_to_set);
  void InitPeerConnectionFactory()
      EXCLUSIVE_LOCKS_REQUIRED(peer_connection_factory_lock_);
  void InitNetworkThread(base::OnceClosure complete_callback);
  void InitSignalingThread(base::OnceClosure complete_callback);
  void InitWorkerThread(base::OnceClosure complete_callback);
  void ShutdownNetworkManager();

  // base::Thread is required here because we need to be able to Start/Stop the
  // threads explicitly with along with the peer connection factory instance.
  base::Thread chrome_network_thread_;
  base::Thread chrome_signaling_thread_;
  base::Thread chrome_worker_thread_;

  // These rtc::Thread* are jingle thread wrappers around the corresponding
  // base::Thread. They get cleaned up on thread shutdown so we don't need to
  // manage lifetime.
  // RAW_PTR_EXCLUSION: Performance.
  RAW_PTR_EXCLUSION rtc::Thread* rtc_network_thread_ = nullptr;
  RAW_PTR_EXCLUSION rtc::Thread* rtc_signaling_thread_ = nullptr;
  RAW_PTR_EXCLUSION rtc::Thread* rtc_worker_thread_ = nullptr;

  // Used to guard access to peer_connection_factory_.
  base::Lock peer_connection_factory_lock_;
  // This factory is shared between all clients, but only initialized once on
  // the passed task runner the first time a Peer Connection is requested.
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_ GUARDED_BY(peer_connection_factory_lock_);

  mojo::SharedRemote<network::mojom::P2PSocketManager> p2p_socket_manager_;
  mojo::SharedRemote<::sharing::mojom::MdnsResponderFactory>
      mdns_responder_factory_;
  mojo::SharedRemote<::sharing::mojom::IceConfigFetcher> ice_config_fetcher_;
  mojo::SharedRemote<::sharing::mojom::WebRtcSignalingMessenger>
      webrtc_signaling_messenger_;

  std::unique_ptr<::sharing::IpcPacketSocketFactory> socket_factory_;
  std::unique_ptr<rtc::NetworkManager> network_manager_;

  // This task runner is used to fetch ice servers and initialize the peer
  // connection factory.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<WebRtcMedium> weak_ptr_factory_{this};
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WEBRTC_H_
