// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/webrtc.h"

#include "base/i18n/timezone.h"
#include "chrome/services/sharing/webrtc/ipc_network_manager.h"
#include "chrome/services/sharing/webrtc/ipc_packet_socket_factory.h"
#include "chrome/services/sharing/webrtc/mdns_responder_adapter.h"
#include "chrome/services/sharing/webrtc/p2p_port_allocator.h"
#include "chromeos/services/nearby/public/mojom/webrtc_signaling_messenger.mojom-shared.h"
#include "jingle/glue/thread_wrapper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/nearby/src/cpp/platform/public/future.h"
#include "third_party/nearby/src/cpp/platform/public/logging.h"
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

// This object only exists to forward incoming mojo messages. It will be created
// as a SelfOwnedReceiver on a separate sequence and will be cleaned up when the
// connection goes down. This is necessary to keep it pumping messages while the
// the main WebRtc thread is blocked on a future.
class IncomingMessageListener
    : public sharing::mojom::IncomingMessagesListener {
 public:
  explicit IncomingMessageListener(
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback
          signaling_message_callback,
      api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
          signaling_complete_callback)
      : signaling_message_callback_(std::move(signaling_message_callback)),
        signaling_complete_callback_(std::move(signaling_complete_callback)) {
    DCHECK(signaling_message_callback_);
    DCHECK(signaling_complete_callback_);
  }

  ~IncomingMessageListener() override = default;

  // mojom::IncomingMessagesListener:
  void OnMessage(const std::string& message) override {
    signaling_message_callback_(ByteArray(message));
  }

  // mojom::IncomingMessagesListener:
  void OnComplete(bool success) override {
    signaling_complete_callback_(success);
  }

 private:
  api::WebRtcSignalingMessenger::OnSignalingMessageCallback
      signaling_message_callback_;
  api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
      signaling_complete_callback_;
};

// Used as a messenger in sending and receiving WebRTC messages between devices.
// The messages sent and received are considered untrusted since they
// originate in an untrusted sandboxed process on device.
class WebRtcSignalingMessengerImpl : public api::WebRtcSignalingMessenger {
 public:
  WebRtcSignalingMessengerImpl(
      const std::string& self_id,
      const connections::LocationHint& location_hint,
      const mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger>&
          messenger)
      : self_id_(self_id),
        location_hint_(location_hint),
        messenger_(messenger),
        task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}

  ~WebRtcSignalingMessengerImpl() override { StopReceivingMessages(); }

  WebRtcSignalingMessengerImpl(const WebRtcSignalingMessengerImpl& other) =
      delete;
  WebRtcSignalingMessengerImpl& operator=(
      const WebRtcSignalingMessengerImpl& other) = delete;

  sharing::mojom::LocationHintPtr CreateLocationHint() {
    sharing::mojom::LocationHintPtr location_hint_ptr =
        sharing::mojom::LocationHint::New();
    location_hint_ptr->location = location_hint_.location();
    switch (location_hint_.format()) {
      case location::nearby::connections::LocationStandard_Format::
          LocationStandard_Format_E164_CALLING:
        location_hint_ptr->format =
            sharing::mojom::LocationStandardFormat::E164_CALLING;
        break;
      case location::nearby::connections::LocationStandard_Format::
          LocationStandard_Format_ISO_3166_1_ALPHA_2:
        location_hint_ptr->format =
            sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2;
        break;
      case location::nearby::connections::LocationStandard_Format::
          LocationStandard_Format_UNKNOWN:
        // Here we default to the current default country code before sending.
        location_hint_ptr->location = base::CountryCodeForCurrentTimezone();
        location_hint_ptr->format =
            sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2;
        break;
    }
    return location_hint_ptr;
  }

  // api::WebRtcSignalingMessenger:
  bool SendMessage(absl::string_view peer_id,
                   const ByteArray& message) override {
    bool success = false;
    if (!messenger_->SendMessage(self_id_, std::string(peer_id),
                                 CreateLocationHint(), std::string(message),
                                 &success)) {
      return false;
    }

    return success;
  }

  void BindIncomingReceiver(
      mojo::PendingReceiver<sharing::mojom::IncomingMessagesListener>
          pending_receiver,
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback
          message_callback,
      api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
          complete_callback) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<IncomingMessageListener>(std::move(message_callback),
                                                  std::move(complete_callback)),
        std::move(pending_receiver), task_runner_);
  }

  // api::WebRtcSignalingMessenger:
  bool StartReceivingMessages(
      OnSignalingMessageCallback message_callback,
      OnSignalingCompleteCallback complete_callback) override {
    bool success = false;
    mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
        pending_remote;
    mojo::PendingReceiver<sharing::mojom::IncomingMessagesListener>
        pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();
    // NOTE: this is a Sync mojo call that waits until Fast-Path ready is
    // received on the Instant Messaging (Tachyon) stream before returning.
    if (!messenger_->StartReceivingMessages(self_id_, CreateLocationHint(),
                                            std::move(pending_remote), &success,
                                            &pending_session_remote_) ||
        !success) {
      receiving_messages_ = false;
      return false;
    }

    // Do the pending_receiver Bind call on the task runner itself so it can
    // receive messages while the WebRtc thread is waiting. Any incoming
    // messages will be queued until the Bind happens.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebRtcSignalingMessengerImpl::BindIncomingReceiver,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(pending_receiver), std::move(message_callback),
                       std::move(complete_callback)));

    receiving_messages_ = true;
    return true;
  }

  // api::WebRtcSignalingMessenger:
  void StopReceivingMessages() override {
    if (receiving_messages_) {
      receiving_messages_ = false;
      if (pending_session_remote_) {
        mojo::Remote<sharing::mojom::ReceiveMessagesSession> session(
            std::move(pending_session_remote_));
        // This is a one-way message so it is safe to bind, send, and forget.
        // When the Remote goes out of scope it will close the pipe and cause
        // the other side to clean up the ReceiveMessagesExpress instance.
        // If the receiver/pipe is already down, this does nothing.
        session->StopReceivingMessages();
      }
    }
  }

 private:
  bool receiving_messages_ = false;
  std::string self_id_;
  connections::LocationHint location_hint_;
  // This is received and stored on a successful StartReceiveMessages(). We
  // choose to not bind right away because multiple threads end up
  // creating/calling/destroying WebRtcSignalingMessengerImpl by the design
  // of NearbyConnections. We need to ensure the thread that
  // binds/calls/destroys the remote is the same sequence, so we do all three at
  // once in StopReceivingMessages(). If the other side of the pipe is already
  // down, binding, calling, and destorying will be a no-op.
  mojo::PendingRemote<sharing::mojom::ReceiveMessagesSession>
      pending_session_remote_;
  mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger> messenger_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<WebRtcSignalingMessengerImpl> weak_ptr_factory_{this};
};

}  // namespace

WebRtcMedium::WebRtcMedium(
    const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager,
    const mojo::SharedRemote<
        location::nearby::connections::mojom::MdnsResponderFactory>&
        mdns_responder_factory,
    const mojo::SharedRemote<sharing::mojom::IceConfigFetcher>&
        ice_config_fetcher,
    const mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger>&
        webrtc_signaling_messenger,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : p2p_socket_manager_(socket_manager),
      mdns_responder_factory_(mdns_responder_factory),
      ice_config_fetcher_(ice_config_fetcher),
      webrtc_signaling_messenger_(webrtc_signaling_messenger),
      task_runner_(std::move(task_runner)) {
  DCHECK(p2p_socket_manager_.is_bound());
  DCHECK(mdns_responder_factory.is_bound());
  DCHECK(ice_config_fetcher_.is_bound());
  DCHECK(webrtc_signaling_messenger_.is_bound());
}

WebRtcMedium::~WebRtcMedium() = default;

const std::string WebRtcMedium::GetDefaultCountryCode() {
  return base::CountryCodeForCurrentTimezone();
}

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
  mojo::PendingRemote<network::mojom::MdnsResponder> pending_remote;
  mojo::PendingReceiver<network::mojom::MdnsResponder> pending_receiver(
      pending_remote.InitWithNewPipeAndPassReceiver());
  mojo::Remote<network::mojom::MdnsResponder> mdns_responder{
      std::move(pending_remote)};
  // We don't need to wait for this call to finish (it doesn't have a callback
  // anyways). The mojo pipe will queue up calls and dispatch as soon as the
  // the other side is available.
  mdns_responder_factory_->CreateMdnsResponder(std::move(pending_receiver));

  ice_config_fetcher_->GetIceServers(base::BindOnce(
      &WebRtcMedium::OnIceServersFetched, weak_ptr_factory_.GetWeakPtr(),
      observer, std::move(callback), std::move(mdns_responder)));
}

void WebRtcMedium::OnIceServersFetched(
    webrtc::PeerConnectionObserver* observer,
    PeerConnectionCallback callback,
    mojo::Remote<network::mojom::MdnsResponder> mdns_responder,
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

  if (!network_manager_) {
    // NOTE: |network_manager_| is only created once and shared for every peer
    // connection due to the use of the shared |p2p_socket_manager_|. See
    // https://crbug.com/1142717 for more details.
    network_manager_ = std::make_unique<sharing::IpcNetworkManager>(
        p2p_socket_manager_, std::make_unique<sharing::MdnsResponderAdapter>(
                                 std::move(mdns_responder)));

    // NOTE: IpcNetworkManager::Initialize() does not override the empty default
    // implementation so this doesn't actually do anything right now. However
    // the contract of rtc::NetworkManagerBase states that it should be called
    // before using and explicitly on the network thread (which right now is the
    // current thread). Previously this was handled by P2PPortAllocator.
    network_manager_->Initialize();
  }

  webrtc::PeerConnectionDependencies dependencies(observer);
  sharing::P2PPortAllocator::Config port_config;
  port_config.enable_multiple_routes = true;
  port_config.enable_nonproxied_udp = true;
  dependencies.allocator = std::make_unique<sharing::P2PPortAllocator>(
      network_manager_.get(), socket_factory_.get(), port_config);
  dependencies.async_resolver_factory =
      std::make_unique<ProxyAsyncResolverFactory>(socket_factory_.get());

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection =
      pc_factory->CreatePeerConnection(rtc_config, std::move(dependencies));
  callback(std::move(peer_connection));
}

std::unique_ptr<api::WebRtcSignalingMessenger>
WebRtcMedium::GetSignalingMessenger(
    absl::string_view self_id,
    const connections::LocationHint& location_hint) {
  return std::make_unique<WebRtcSignalingMessengerImpl>(
      std::string(self_id), location_hint, webrtc_signaling_messenger_);
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
