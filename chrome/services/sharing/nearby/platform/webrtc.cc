// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/webrtc.h"

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/services/sharing/webrtc/ipc_network_manager.h"
#include "chrome/services/sharing/webrtc/ipc_packet_socket_factory.h"
#include "chrome/services/sharing/webrtc/mdns_responder_adapter.h"
#include "chrome/services/sharing/webrtc/p2p_port_allocator.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc_signaling_messenger.mojom-shared.h"
#include "components/webrtc/thread_wrapper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/nearby/src/internal/platform/count_down_latch.h"
#include "third_party/nearby/src/internal/platform/future.h"
#include "third_party/nearby/src/internal/platform/logging.h"
#include "third_party/webrtc/api/async_dns_resolver.h"
#include "third_party/webrtc/api/jsep.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"
#include "unicode/locid.h"

namespace nearby::chrome {

namespace {

// The following constants are RTCConfiguration defaults designed to help with
// battery life for persistent connections like Phone Hub. These values were
// chosen by doing battery drain tests on an Android phone with a persistent
// Phone Hub connection upgraded to WebRtc. The goal here is prevent chatty
// KeepAlive pings at the WebRtc layer from waking up the Phone's kernel too
// frequently and causing battery drain.
//
// NOTE: Nearby Connections also has its own KeepAlive interval and timeout that
// are different from these core WebRtc values. They operate at a different
// layer and don't directly affect the values chosen for WebRtc. However, both
// values need to be greater than the defaults for battery saving to happen and
// the most frequent ping ultimately determines the worst case number of wake
// ups.
//
// See: b/183505430 for more context.
constexpr base::TimeDelta kIceConnectionReceivingTimeout = base::Minutes(10);
constexpr base::TimeDelta kIceCheckIntervalStrongConnectivity =
    base::Seconds(25);
constexpr base::TimeDelta kStableWritableConnectionPingInterval =
    base::Seconds(30);

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

// Returns the ISO country code for the locale currently set as the
// user's device language.
const std::string GetCurrentCountryCode() {
  return std::string(icu::Locale::getDefault().getCountry());
}

class ProxyAsyncDnsResolverFactory final
    : public webrtc::AsyncDnsResolverFactoryInterface {
 public:
  explicit ProxyAsyncDnsResolverFactory(
      sharing::IpcPacketSocketFactory* socket_factory)
      : socket_factory_(socket_factory) {
    DCHECK(socket_factory_);
  }

  std::unique_ptr<webrtc::AsyncDnsResolverInterface> Create() override {
    return socket_factory_->CreateAsyncDnsResolver();
  }
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndResolve(
      const rtc::SocketAddress& addr,
      absl::AnyInvocable<void()> callback) override {
    auto temp = Create();
    temp->Start(addr, std::move(callback));
    return temp;
  }
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndResolve(
      const rtc::SocketAddress& addr,
      int family,
      absl::AnyInvocable<void()> callback) override {
    auto temp = Create();
    temp->Start(addr, family, std::move(callback));
    return temp;
  }

 private:
  raw_ptr<sharing::IpcPacketSocketFactory> socket_factory_;
};

// This object only exists to forward incoming mojo messages. It will be created
// as a SelfOwnedReceiver on a separate sequence and will be cleaned up when the
// connection goes down. This is necessary to keep it pumping messages while the
// the main WebRtc thread is blocked on a future.
class IncomingMessageListener
    : public ::sharing::mojom::IncomingMessagesListener {
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
      const location::nearby::connections::LocationHint& location_hint,
      const mojo::SharedRemote<::sharing::mojom::WebRtcSignalingMessenger>&
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

  ::sharing::mojom::LocationHintPtr CreateLocationHint() {
    ::sharing::mojom::LocationHintPtr location_hint_ptr =
        ::sharing::mojom::LocationHint::New();
    location_hint_ptr->location = location_hint_.location();
    switch (location_hint_.format()) {
      case location::nearby::connections::LocationStandard_Format::
          LocationStandard_Format_E164_CALLING:
        location_hint_ptr->format =
            ::sharing::mojom::LocationStandardFormat::E164_CALLING;
        break;
      case location::nearby::connections::LocationStandard_Format::
          LocationStandard_Format_ISO_3166_1_ALPHA_2:
        location_hint_ptr->format =
            ::sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2;
        break;
      case location::nearby::connections::LocationStandard_Format::
          LocationStandard_Format_UNKNOWN:
        // Here we default to the current default country code before sending.
        location_hint_ptr->location = GetCurrentCountryCode();
        location_hint_ptr->format =
            ::sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2;
        break;
    }
    return location_hint_ptr;
  }

  // api::WebRtcSignalingMessenger:
  bool SendMessage(std::string_view peer_id,
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
      mojo::PendingReceiver<::sharing::mojom::IncomingMessagesListener>
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
    mojo::PendingRemote<::sharing::mojom::IncomingMessagesListener>
        pending_remote;
    mojo::PendingReceiver<::sharing::mojom::IncomingMessagesListener>
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
        mojo::Remote<::sharing::mojom::ReceiveMessagesSession> session(
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
  location::nearby::connections::LocationHint location_hint_;
  // This is received and stored on a successful StartReceiveMessages(). We
  // choose to not bind right away because multiple threads end up
  // creating/calling/destroying WebRtcSignalingMessengerImpl by the design
  // of NearbyConnections. We need to ensure the thread that
  // binds/calls/destroys the remote is the same sequence, so we do all three at
  // once in StopReceivingMessages(). If the other side of the pipe is already
  // down, binding, calling, and destroying will be a no-op.
  mojo::PendingRemote<::sharing::mojom::ReceiveMessagesSession>
      pending_session_remote_;
  mojo::SharedRemote<::sharing::mojom::WebRtcSignalingMessenger> messenger_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<WebRtcSignalingMessengerImpl> weak_ptr_factory_{this};
};

}  // namespace

WebRtcMedium::WebRtcMedium(
    const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager,
    const mojo::SharedRemote<::sharing::mojom::MdnsResponderFactory>&
        mdns_responder_factory,
    const mojo::SharedRemote<::sharing::mojom::IceConfigFetcher>&
        ice_config_fetcher,
    const mojo::SharedRemote<::sharing::mojom::WebRtcSignalingMessenger>&
        webrtc_signaling_messenger,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : chrome_network_thread_(/*name=*/"WebRtc Network Thread"),
      chrome_signaling_thread_(/*name=*/"WebRtc Signaling Thread"),
      chrome_worker_thread_(/*name=*/"WebRtc Worker Thread"),
      p2p_socket_manager_(socket_manager),
      mdns_responder_factory_(mdns_responder_factory),
      ice_config_fetcher_(ice_config_fetcher),
      webrtc_signaling_messenger_(webrtc_signaling_messenger),
      task_runner_(std::move(task_runner)) {
  DCHECK(p2p_socket_manager_.is_bound());
  DCHECK(mdns_responder_factory.is_bound());
  DCHECK(ice_config_fetcher_.is_bound());
  DCHECK(webrtc_signaling_messenger_.is_bound());
}

WebRtcMedium::~WebRtcMedium() {
  VLOG(1) << "WebRtcMedium destructor is running";
  // In case initialization was pending on another thread we block waiting for
  // the lock before we clear peer_connection_factory_.
  base::AutoLock peer_connection_factory_auto_lock(
      peer_connection_factory_lock_);
  peer_connection_factory_ = nullptr;

  if (chrome_network_thread_.IsRunning()) {
    // The network manager needs to free its resources on the thread they were
    // created, which is the network thread.
    if (network_manager_ || p2p_socket_manager_) {
      chrome_network_thread_.task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&WebRtcMedium::ShutdownNetworkManager,
                                    weak_ptr_factory_.GetWeakPtr()));
    }
    // Stopping the thread will wait until all tasks have been
    // processed before returning. We wait for the above task to finish before
    // letting the the function continue to ensure network_manager_ is cleaned
    // up if it needed to be.
    chrome_network_thread_.Stop();
    DCHECK(!network_manager_);
    DCHECK(!socket_factory_);
  }

  // Stop is called in thread destructor, but we want to ensure all threads are
  // down before the destructor is complete and we release the lock.
  chrome_signaling_thread_.Stop();
  chrome_worker_thread_.Stop();
  VLOG(1) << "WebRtcMedium destructor is done shutting down threads.";
}

const std::string WebRtcMedium::GetDefaultCountryCode() {
  return GetCurrentCountryCode();
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
  ice_config_fetcher_->GetIceServers(base::BindOnce(
      &WebRtcMedium::OnIceServersFetched, weak_ptr_factory_.GetWeakPtr(),
      base::UnsafeDanglingUntriaged(observer), std::move(callback)));
}

void WebRtcMedium::InitWebRTCThread(rtc::Thread** thread_to_set) {
  webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
  webrtc::ThreadWrapper::current()->set_send_allowed(true);
  *thread_to_set = webrtc::ThreadWrapper::current();
}

void WebRtcMedium::InitPeerConnectionFactory() {
  DCHECK(!chrome_network_thread_.IsRunning());
  DCHECK(!chrome_signaling_thread_.IsRunning());
  DCHECK(!chrome_worker_thread_.IsRunning());
  DCHECK(!rtc_network_thread_);
  DCHECK(!rtc_signaling_thread_);
  DCHECK(!rtc_worker_thread_);

  webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
  webrtc::ThreadWrapper::current()->set_send_allowed(true);

  // We need to create three dedicated threads for WebRTC. We post tasks to the
  // threads and to ensure the message loop and jingle wrapper is setup for each
  // thread. Unretained(this) is used because we will wait on this thread for
  // the tasks to complete before exiting.

  CountDownLatch latch(3);
  auto decrement_latch = base::BindRepeating(
      [](CountDownLatch* latch) { latch->CountDown(); }, &latch);

  chrome_network_thread_.Start();
  chrome_network_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcMedium::InitNetworkThread,
                                base::Unretained(this), decrement_latch));

  chrome_worker_thread_.Start();
  chrome_worker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcMedium::InitWorkerThread,
                                base::Unretained(this), decrement_latch));

  chrome_signaling_thread_.Start();
  chrome_signaling_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcMedium::InitSignalingThread,
                                base::Unretained(this), decrement_latch));

  // Wait for all threads to be initialized
  latch.Await();

  DCHECK(rtc_network_thread_);
  DCHECK(rtc_signaling_thread_);
  DCHECK(rtc_worker_thread_);

  webrtc::PeerConnectionFactoryDependencies factory_dependencies;
  factory_dependencies.task_queue_factory = CreateWebRtcTaskQueueFactory();
  factory_dependencies.network_thread = rtc_network_thread_;
  factory_dependencies.worker_thread = rtc_worker_thread_;
  factory_dependencies.signaling_thread = rtc_signaling_thread_;

  peer_connection_factory_ = webrtc::CreateModularPeerConnectionFactory(
      std::move(factory_dependencies));
}

void WebRtcMedium::InitNetworkThread(base::OnceClosure complete_callback) {
  DCHECK(chrome_network_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!rtc_network_thread_);
  DCHECK(!network_manager_);
  DCHECK(!socket_factory_);

  InitWebRTCThread(&rtc_network_thread_);

  // Get a connection to the mdns responder from the factory interface
  mojo::PendingRemote<network::mojom::MdnsResponder> pending_remote;
  mojo::PendingReceiver<network::mojom::MdnsResponder> pending_receiver(
      pending_remote.InitWithNewPipeAndPassReceiver());
  mojo::Remote<network::mojom::MdnsResponder> mdns_responder{
      std::move(pending_remote)};

  // We don't need to wait for this call to finish (it doesn't have a callback
  // anyways). The mojo pipe will queue up calls and dispatch as soon as the
  // the other side is available.
  mdns_responder_factory_->CreateMdnsResponder(std::move(pending_receiver));

  network_manager_ = std::make_unique<sharing::IpcNetworkManager>(
      p2p_socket_manager_, std::make_unique<sharing::MdnsResponderAdapter>(
                               std::move(mdns_responder)));

  socket_factory_ = std::make_unique<sharing::IpcPacketSocketFactory>(
      p2p_socket_manager_, kTrafficAnnotation);

  // NOTE: IpcNetworkManager::Initialize() does not override the empty default
  // implementation so this doesn't actually do anything right now. However
  // the contract of rtc::NetworkManagerBase states that it should be called
  // before using and explicitly on the network thread (which right now is the
  // current thread). Previously this was handled by P2PPortAllocator.
  network_manager_->Initialize();

  std::move(complete_callback).Run();
}

void WebRtcMedium::ShutdownNetworkManager() {
  DCHECK(chrome_network_thread_.task_runner()->BelongsToCurrentThread());
  network_manager_.reset();
  socket_factory_.reset();
}

void WebRtcMedium::InitSignalingThread(base::OnceClosure complete_callback) {
  DCHECK(chrome_signaling_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!rtc_signaling_thread_);

  InitWebRTCThread(&rtc_signaling_thread_);

  std::move(complete_callback).Run();
}

void WebRtcMedium::InitWorkerThread(base::OnceClosure complete_callback) {
  DCHECK(chrome_worker_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!rtc_worker_thread_);

  InitWebRTCThread(&rtc_worker_thread_);

  std::move(complete_callback).Run();
}

void WebRtcMedium::OnIceServersFetched(
    webrtc::PeerConnectionObserver* observer,
    PeerConnectionCallback callback,
    std::vector<::sharing::mojom::IceServerPtr> ice_servers) {
  base::AutoLock peer_connection_factory_auto_lock(
      peer_connection_factory_lock_);
  if (!peer_connection_factory_) {
    InitPeerConnectionFactory();
  }

  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  // Use the spec-compliant SDP semantics.
  rtc_config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  // Add |ice_servers| into the rtc_config.servers.
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

  // This prevents WebRTC from being chatty with keep alive messages which was
  // causing battery drain for Phone Hub's persistent connection.
  // Ideally these options should be configurable per connection, but right now
  // we have a single share factory for all peer connections.
  if (ash::features::IsNearbyKeepAliveFixEnabled()) {
    rtc_config.ice_connection_receiving_timeout =
        kIceConnectionReceivingTimeout.InMilliseconds();
    rtc_config.ice_check_interval_strong_connectivity =
        kIceCheckIntervalStrongConnectivity.InMilliseconds();
    rtc_config.stable_writable_connection_ping_interval_ms =
        kStableWritableConnectionPingInterval.InMilliseconds();
  }

  webrtc::PeerConnectionDependencies dependencies(observer);
  sharing::P2PPortAllocator::Config port_config;
  port_config.enable_multiple_routes = true;
  port_config.enable_nonproxied_udp = true;
  dependencies.allocator = std::make_unique<sharing::P2PPortAllocator>(
      network_manager_.get(), socket_factory_.get(), port_config);
  dependencies.async_dns_resolver_factory =
      std::make_unique<ProxyAsyncDnsResolverFactory>(socket_factory_.get());

  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>
      peer_connection = peer_connection_factory_->CreatePeerConnectionOrError(
          rtc_config, std::move(dependencies));
  if (peer_connection.ok()) {
    callback(peer_connection.MoveValue());
  } else {
    callback(/*peer_connection=*/nullptr);
  }
}

std::unique_ptr<api::WebRtcSignalingMessenger>
WebRtcMedium::GetSignalingMessenger(
    std::string_view self_id,
    const location::nearby::connections::LocationHint& location_hint) {
  return std::make_unique<WebRtcSignalingMessengerImpl>(
      std::string(self_id), location_hint, webrtc_signaling_messenger_);
}

}  // namespace nearby::chrome
