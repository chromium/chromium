// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/webrtc.h"

#include "chrome/services/sharing/webrtc/ipc_network_manager.h"
#include "chrome/services/sharing/webrtc/ipc_packet_socket_factory.h"
#include "chrome/services/sharing/webrtc/mdns_responder_adapter.h"
#include "chrome/services/sharing/webrtc/p2p_port_allocator.h"
#include "jingle/glue/thread_wrapper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/nearby/src/cpp/platform_v2/public/future.h"
#include "third_party/webrtc/api/jsep.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace location {
namespace nearby {
namespace chrome {

namespace {
net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("nearby_webrtc_connection", R"(
        semantics {
          sender: "Chrome Nearby Share via WebRTC"
          description:
            "Chrome Nearby Share allows users to send data securely between "
            "devices. WebRTC allows Chrome to establish a secure session with "
            "another Nearby instance running on a different device and to "
            "transmit and receive data that users want to share across "
            "devices."
          trigger:
            "User uses the Nearby Share feature and selects a peer device to"
            " send the data to."
          data:
            "Text and media encrypted via AES-256-CBC. Protocol-level messages "
            "for the various subprotocols employed by WebRTC (including ICE, "
            "DTLS, RTCP, etc.) are encrypted via DTLS-SRTP. Note that ICE "
            "connectivity checks may leak the user's IP address(es), subject "
            "to the restrictions/guidance in "
            "https://datatracker.ietf.org/doc/draft-ietf-rtcweb-ip-handling."
          destination: OTHER
          destination_other:
            "A peer Nearby device that receives this data"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only enabled for signed-in users who enable "
            "Nearby Share"
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        }
    )");

class ProxyAsyncResolverFactory final : public webrtc::AsyncResolverFactory {
 public:
  explicit ProxyAsyncResolverFactory(
      sharing::IpcPacketSocketFactory* socket_factory)
      : socket_factory_(socket_factory) {
    DCHECK(socket_factory_);
  }

  rtc::AsyncResolverInterface* Create() override {
    return socket_factory_->CreateAsyncResolver();
  }

 private:
  sharing::IpcPacketSocketFactory* socket_factory_;
};

// Used as a messenger in sending and receiving WebRTC messages between devices.
// The messages sent and received are considered untrusted since they
// originate in an untrusted sandboxed process on device.
class WebRtcSignalingMessengerImpl
    : public api::WebRtcSignalingMessenger,
      public sharing::mojom::IncomingMessagesListener {
 public:
  using OnSignalingMessageCallback =
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback;

  WebRtcSignalingMessengerImpl(
      const std::string& self_id,
      const mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger>&
          messenger)
      : self_id_(self_id), messenger_(messenger) {}

  ~WebRtcSignalingMessengerImpl() override = default;

  WebRtcSignalingMessengerImpl(const WebRtcSignalingMessengerImpl& other) =
      delete;
  WebRtcSignalingMessengerImpl& operator=(
      const WebRtcSignalingMessengerImpl& other) = delete;

  // api::WebRtcSignalingMessenger:
  bool SendMessage(absl::string_view peer_id,
                   const ByteArray& message) override {
    bool success = false;
    if (!messenger_->SendMessage(self_id_, std::string(peer_id),
                                 std::string(message), &success)) {
      return false;
    }

    return success;
  }

  // api::WebRtcSignalingMessenger:
  bool StartReceivingMessages(OnSignalingMessageCallback callback) override {
    signaling_message_callback_ = std::move(callback);
    incoming_messages_receiver_.reset();
    bool success = false;

    if (!messenger_->StartReceivingMessages(
            self_id_, incoming_messages_receiver_.BindNewPipeAndPassRemote(),
            &success) ||
        !success) {
      incoming_messages_receiver_.reset();
      signaling_message_callback_ = nullptr;
      return false;
    }

    incoming_messages_receiver_.set_disconnect_handler(
        base::BindOnce(&WebRtcSignalingMessengerImpl::StopReceivingMessages,
                       base::Unretained(this)));

    return success;
  }

  // api::WebRtcSignalingMessenger:
  void StopReceivingMessages() override {
    incoming_messages_receiver_.reset();
    signaling_message_callback_ = nullptr;
    messenger_->StopReceivingMessages();
  }

 private:
  // mojom::IncomingMessagesListener:
  void OnMessage(const std::string& message) override {
    if (signaling_message_callback_)
      signaling_message_callback_(ByteArray(message));
  }

  std::string self_id_;
  mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger> messenger_;
  mojo::Receiver<sharing::mojom::IncomingMessagesListener>
      incoming_messages_receiver_{this};
  OnSignalingMessageCallback signaling_message_callback_;

  base::WeakPtrFactory<WebRtcSignalingMessengerImpl> weak_ptr_factory_{this};
};

}  // namespace

WebRtcMedium::WebRtcMedium(
    const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager,
    const mojo::SharedRemote<network::mojom::MdnsResponder>& mdns_responder,
    const mojo::SharedRemote<sharing::mojom::IceConfigFetcher>&
        ice_config_fetcher,
    const mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger>&
        webrtc_signaling_messenger,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : p2p_socket_manager_(socket_manager),
      mdns_responder_(mdns_responder),
      ice_config_fetcher_(ice_config_fetcher),
      webrtc_signaling_messenger_(webrtc_signaling_messenger),
      task_runner_(std::move(task_runner)) {
  DCHECK(p2p_socket_manager_.is_bound());
  DCHECK(mdns_responder_.is_bound());
  DCHECK(ice_config_fetcher_.is_bound());
  DCHECK(webrtc_signaling_messenger_.is_bound());
}

WebRtcMedium::~WebRtcMedium() = default;

void WebRtcMedium::CreatePeerConnection(
    webrtc::PeerConnectionObserver* observer,
    PeerConnectionCallback callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcMedium::FetchIceServers,
                                weak_ptr_factory_.GetWeakPtr(), observer,
                                std::move(callback)));
}

void WebRtcMedium::FetchIceServers(webrtc::PeerConnectionObserver* observer,
                                   PeerConnectionCallback callback) {
  ice_config_fetcher_->GetIceServers(base::BindOnce(
      &WebRtcMedium::OnIceServersFetched, weak_ptr_factory_.GetWeakPtr(),
      observer, std::move(callback)));
}

void WebRtcMedium::OnIceServersFetched(
    webrtc::PeerConnectionObserver* observer,
    PeerConnectionCallback callback,
    std::vector<sharing::mojom::IceServerPtr> ice_servers) {
  // WebRTC is using the current thread instead of creating new threads since
  // otherwise the |network_manager| is created on current thread and destroyed
  // on network thread, and so the mojo Receiver stored in it is not called on
  // the same sequence. The long terms correct fix is to resolve
  // http://crbug.com/1044522 and reuse the code in blink layer which ensures
  // that the objects are created on the same thread they would be destroyed in.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  jingle_glue::JingleThreadWrapper::current()->set_send_allowed(true);

  webrtc::PeerConnectionFactoryDependencies factory_dependencies;
  factory_dependencies.task_queue_factory = CreateWebRtcTaskQueueFactory();
  factory_dependencies.network_thread = rtc::Thread::Current();
  factory_dependencies.worker_thread = rtc::Thread::Current();
  factory_dependencies.signaling_thread = rtc::Thread::Current();

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory =
      webrtc::CreateModularPeerConnectionFactory(
          std::move(factory_dependencies));

  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  for (const auto& ice_server : ice_servers) {
    webrtc::PeerConnectionInterface::IceServer ice_turn_server;
    for (const auto& url : ice_server->urls)
      ice_turn_server.urls.push_back(url.spec());
    if (ice_server->username)
      ice_turn_server.username = *ice_server->username;
    if (ice_server->credential)
      ice_turn_server.password = *ice_server->credential;
    rtc_config.servers.push_back(ice_turn_server);
  }

  if (!socket_factory_) {
    socket_factory_ = std::make_unique<sharing::IpcPacketSocketFactory>(
        p2p_socket_manager_, kTrafficAnnotation);
  }

  auto network_manager = std::make_unique<sharing::IpcNetworkManager>(
      p2p_socket_manager_,
      std::make_unique<sharing::MdnsResponderAdapter>(mdns_responder_));

  webrtc::PeerConnectionDependencies dependencies(observer);
  sharing::P2PPortAllocator::Config port_config;
  port_config.enable_multiple_routes = true;
  port_config.enable_nonproxied_udp = true;
  dependencies.allocator = std::make_unique<sharing::P2PPortAllocator>(
      std::move(network_manager), socket_factory_.get(), port_config);
  dependencies.async_resolver_factory =
      std::make_unique<ProxyAsyncResolverFactory>(socket_factory_.get());

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection =
      pc_factory->CreatePeerConnection(rtc_config, std::move(dependencies));
  callback(std::move(peer_connection));
}

std::unique_ptr<api::WebRtcSignalingMessenger>
WebRtcMedium::GetSignalingMessenger(absl::string_view self_id) {
  return std::make_unique<WebRtcSignalingMessengerImpl>(
      std::string(self_id), webrtc_signaling_messenger_);
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
