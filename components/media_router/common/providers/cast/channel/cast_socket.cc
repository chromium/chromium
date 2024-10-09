// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_socket.h"

#include <stdlib.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/media_router/common/providers/cast/channel/cast_auth_util.h"
#include "components/media_router/common/providers/cast/channel/cast_framer.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_transport.h"
#include "components/media_router/common/providers/cast/channel/keep_alive_delegate.h"
#include "components/media_router/common/providers/cast/channel/logger.h"
#include "components/media_router/common/providers/cast/channel/mojo_data_pump.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

// Helper for logging data with remote host IP and authentication state.
// Assumes |ip_endpoint_| of type net::IPEndPoint and |channel_auth_| of enum
// type ChannelAuthType are available in the current scope.
#define CONNECTION_INFO()                                             \
  "[" << open_params_.ip_endpoint.ToString() << ", auth=SSL_VERIFIED" \
      << "] "
#define VLOG_WITH_CONNECTION(level) VLOG(level) << CONNECTION_INFO()
#define LOG_WITH_CONNECTION(level) LOG(level) << CONNECTION_INFO()

namespace cast_channel {
namespace {

bool IsTerminalState(ConnectionState state) {
  return state == ConnectionState::FINISHED ||
         state == ConnectionState::CONNECT_ERROR ||
         state == ConnectionState::TIMEOUT;
}

void OnConnected(
    network::mojom::NetworkContext::CreateTCPConnectedSocketCallback callback,
    int result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), result, local_addr, peer_addr,
                     std::move(receive_stream), std::move(send_stream)));
}

void ConnectOnUIThread(
    network::NetworkContextGetter network_context_getter,
    const net::AddressList& remote_address_list,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    network::mojom::NetworkContext::CreateTCPConnectedSocketCallback callback) {
  network_context_getter.Run()->CreateTCPConnectedSocket(
      std::nullopt /* local_addr */, remote_address_list,
      nullptr /* tcp_connected_socket_options */,
      net::MutableNetworkTrafficAnnotationTag(
          CastSocketImpl::GetNetworkTrafficAnnotationTag()),
      std::move(receiver), mojo::NullRemote() /* observer */,
      base::BindOnce(OnConnected, std::move(callback)));
}

}  // namespace

CastSocket::Observer::~Observer() {
  CHECK(!IsInObserverList());
}

CastSocketImpl::CastSocketImpl(
    network::NetworkContextGetter network_context_getter,
    const CastSocketOpenParams& open_params,
    const scoped_refptr<Logger>& logger)
    : CastSocketImpl(network_context_getter,
                     open_params,
                     logger,
                     AuthContext::Create()) {}

CastSocketImpl::CastSocketImpl(
    network::NetworkContextGetter network_context_getter,
    const CastSocketOpenParams& open_params,
    const scoped_refptr<Logger>& logger,
    const AuthContext& auth_context)
    : channel_id_(0),
      open_params_(open_params),
      logger_(logger),
      network_context_getter_(network_context_getter),
      auth_context_(auth_context),
      connect_timeout_timer_(new base::OneShotTimer),
      is_canceled_(false),
      audio_only_(false),
      connect_state_(ConnectionState::START_CONNECT),
      error_state_(ChannelError::NONE),
      ready_state_(ReadyState::NONE),
      auth_delegate_(nullptr) {
  DCHECK(open_params.ip_endpoint.address().IsValid());
}

CastSocket::~CastSocket() = default;

CastSocketImpl::~CastSocketImpl() {
  // Ensure that resources are freed but do not run pending callbacks that
  // would result in re-entrancy.
  CloseInternal();

  error_state_ = ChannelError::UNKNOWN;
  for (auto& connect_callback : connect_callbacks_)
    std::move(connect_callback).Run(this);
  connect_callbacks_.clear();
}

ReadyState CastSocketImpl::ready_state() const {
  return ready_state_;
}

ChannelError CastSocketImpl::error_state() const {
  return error_state_;
}

CastChannelFlags CastSocketImpl::flags() const {
  return flags_;
}

const net::IPEndPoint& CastSocketImpl::ip_endpoint() const {
  return open_params_.ip_endpoint;
}

int CastSocketImpl::id() const {
  return channel_id_;
}

void CastSocketImpl::set_id(int id) {
  channel_id_ = id;
}

bool CastSocketImpl::keep_alive() const {
  return open_params_.liveness_timeout.is_positive();
}

bool CastSocketImpl::audio_only() const {
  return audio_only_;
}

bool CastSocketImpl::VerifyChannelPolicy(const AuthResult& result) {
  audio_only_ = (result.channel_policies & AuthResult::POLICY_AUDIO_ONLY) != 0;
  if (audio_only_ &&
      open_params_.device_capabilities.Has(CastDeviceCapability::kVideoOut)) {
    LOG_WITH_CONNECTION(ERROR)
        << "Audio only channel policy enforced for video out capable device";
    return false;
  }
  return true;
}

bool CastSocketImpl::VerifyChallengeReply() {
  DCHECK(peer_cert_);
  AuthResult result =
      AuthenticateChallengeReply(*challenge_reply_, *peer_cert_, auth_context_);
  flags_ = result.flags;
  logger_->LogSocketChallengeReplyEvent(channel_id_, result);
  if (result.success()) {
    VLOG(1) << result.error_message;
    if (!VerifyChannelPolicy(result)) {
      return false;
    }
  }
  return result.success();
}

void CastSocketImpl::SetTransportForTesting(
    std::unique_ptr<CastTransport> transport) {
  transport_ = std::move(transport);
}

void CastSocketImpl::SetPeerCertForTesting(
    scoped_refptr<net::X509Certificate> peer_cert) {
  peer_cert_ = peer_cert;
}

void CastSocketImpl::Connect(OnOpenCallback callback) {
  switch (ready_state_) {
    case ReadyState::NONE:
      connect_callbacks_.push_back(std::move(callback));
      Connect();
      break;
    case ReadyState::CONNECTING:
      connect_callbacks_.push_back(std::move(callback));
      break;
    case ReadyState::OPEN:
      error_state_ = ChannelError::NONE;
      std::move(callback).Run(this);
      break;
    case ReadyState::CLOSED:
      error_state_ = ChannelError::CONNECT_ERROR;
      std::move(callback).Run(this);
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unknown ReadyState: " << ReadyStateToString(ready_state_);
  }
}

void CastSocketImpl::Connect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG_WITH_CONNECTION(1) << "Connect readyState = "
                          << ReadyStateToString(ready_state_);
  DCHECK_EQ(ReadyState::NONE, ready_state_);
  DCHECK_EQ(ConnectionState::START_CONNECT, connect_state_);

  delegate_ = std::make_unique<CastSocketMessageDelegate>(this);

  SetReadyState(ReadyState::CONNECTING);
  SetConnectState(ConnectionState::TCP_CONNECT);

  // Set up connection timeout.
  if (open_params_.connect_timeout.InMicroseconds() > 0) {
    DCHECK(connect_timeout_callback_.IsCancelled());
    connect_timeout_callback_.Reset(base::BindOnce(
        &CastSocketImpl::OnConnectTimeout, base::Unretained(this)));
    GetTimer()->Start(FROM_HERE, open_params_.connect_timeout,
                      connect_timeout_callback_.callback());
  }

  DoConnectLoop(net::OK);
}

CastTransport* CastSocketImpl::transport() const {
  return transport_.get();
}

void CastSocketImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void CastSocketImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

net::NetworkTrafficAnnotationTag
CastSocketImpl::GetNetworkTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("cast_socket", R"(
        semantics {
          sender: "Cast Socket"
          description:
            "Requests to a Cast device."
          trigger:
            "A new Cast device has been discovered via mDNS in the local "
            "network or after it's connected."
          data:
            "A serialized Cast protocol or application-level protobuf message. "
            "A non-exhaustive list of Cast protocol messages:\n"
            "- nonce challenge,\n"
            "- ping/pong data,\n"
            "- Virtual connection requests,\n"
            "- App availability / media status / receiver status requests,\n"
            "- Launch / stop Cast session requests,\n"
            "- Media commands, such as play/pause.\n"
            "Application-level messages may contain data specific to the Cast "
            "application."
          destination: OTHER
          destination_other:
            "Data will be sent to a Cast device in local network."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be disabled in settings, but it would not be "
            "sent if user does not connect a Cast device to the local network."
          chrome_policy {
            EnableMediaRouter {
              EnableMediaRouter: false
            }
          }
        })");
}

void CastSocketImpl::OnConnectTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Stop all pending connection setup tasks and report back to the client.
  is_canceled_ = true;
  VLOG_WITH_CONNECTION(1) << "Timeout while establishing a connection.";
  SetErrorState(ChannelError::CONNECT_TIMEOUT);
  DoConnectCallback();
}

void CastSocketImpl::ResetConnectLoopCallback() {
  DCHECK(connect_loop_callback_.IsCancelled());
  connect_loop_callback_.Reset(
      base::BindOnce(&CastSocketImpl::DoConnectLoop, base::Unretained(this)));
}

void CastSocketImpl::PostTaskToStartConnectLoop(int result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ResetConnectLoopCallback();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(connect_loop_callback_.callback(), result));
}

// This method performs the state machine transitions for connection flow.
// There are two entry points to this method:
// 1. Connect method: this starts the flow
// 2. Callback from network operations that finish asynchronously.
void CastSocketImpl::DoConnectLoop(int result) {
  connect_loop_callback_.Cancel();
  if (is_canceled_) {
    LOG_WITH_CONNECTION(ERROR) << "CANCELLED - Aborting DoConnectLoop.";
    return;
  }

  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // correct state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    ConnectionState state = connect_state_;
    connect_state_ = ConnectionState::UNKNOWN;
    switch (state) {
      case ConnectionState::TCP_CONNECT:
        rv = DoTcpConnect();
        break;
      case ConnectionState::TCP_CONNECT_COMPLETE:
        rv = DoTcpConnectComplete(rv);
        break;
      case ConnectionState::SSL_CONNECT:
        DCHECK_EQ(net::OK, rv);
        rv = DoSslConnect();
        break;
      case ConnectionState::SSL_CONNECT_COMPLETE:
        rv = DoSslConnectComplete(rv);
        break;
      case ConnectionState::AUTH_CHALLENGE_SEND:
        rv = DoAuthChallengeSend();
        break;
      case ConnectionState::AUTH_CHALLENGE_SEND_COMPLETE:
        rv = DoAuthChallengeSendComplete(rv);
        break;
      case ConnectionState::AUTH_CHALLENGE_REPLY_COMPLETE:
        rv = DoAuthChallengeReplyComplete(rv);
        DCHECK(IsTerminalState(connect_state_));
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unknown state in connect flow: " << AsInteger(state);
        SetConnectState(ConnectionState::FINISHED);
        SetErrorState(ChannelError::UNKNOWN);
        DoConnectCallback();
        return;
    }
  } while (rv != net::ERR_IO_PENDING && !IsTerminalState(connect_state_));
  // Exit the state machine if an asynchronous network operation is pending
  // or if the state machine is in the terminal "finished" state.

  if (IsTerminalState(connect_state_)) {
    DCHECK_NE(rv, net::ERR_IO_PENDING);
    GetTimer()->Stop();
    DoConnectCallback();
  } else {
    DCHECK_EQ(rv, net::ERR_IO_PENDING);
  }
}

int CastSocketImpl::DoTcpConnect() {
  DCHECK(connect_loop_callback_.IsCancelled());
  VLOG_WITH_CONNECTION(1) << "DoTcpConnect";
  SetConnectState(ConnectionState::TCP_CONNECT_COMPLETE);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(ConnectOnUIThread, network_context_getter_,
                                net::AddressList(open_params_.ip_endpoint),
                                tcp_socket_.BindNewPipeAndPassReceiver(),
                                base::BindOnce(&CastSocketImpl::OnConnect,
                                               weak_factory_.GetWeakPtr())));

  return net::ERR_IO_PENDING;
}

int CastSocketImpl::DoTcpConnectComplete(int connect_result) {
  VLOG_WITH_CONNECTION(1) << "DoTcpConnectComplete: " << connect_result;
  logger_->LogSocketEventWithRv(
      channel_id_, ChannelEvent::TCP_SOCKET_CONNECT_COMPLETE, connect_result);
  if (connect_result == net::OK) {
    SetConnectState(ConnectionState::SSL_CONNECT);
  } else if (connect_result == net::ERR_CONNECTION_TIMED_OUT) {
    SetConnectState(ConnectionState::FINISHED);
    SetErrorState(ChannelError::CONNECT_TIMEOUT);
  } else {
    SetConnectState(ConnectionState::FINISHED);
    SetErrorState(ChannelError::CONNECT_ERROR);
  }
  return connect_result;
}

int CastSocketImpl::DoSslConnect() {
  DCHECK(connect_loop_callback_.IsCancelled());
  VLOG_WITH_CONNECTION(1) << "DoSslConnect";
  SetConnectState(ConnectionState::SSL_CONNECT_COMPLETE);

  net::HostPortPair host_port_pair =
      net::HostPortPair::FromIPEndPoint(open_params_.ip_endpoint);
  network::mojom::TLSClientSocketOptionsPtr options =
      network::mojom::TLSClientSocketOptions::New();
  // Cast code does its own authentication after SSL handshake since the devices
  // don't have a known hostname.
  options->unsafely_skip_cert_verification = true;
  tcp_socket_->UpgradeToTLS(
      host_port_pair, std::move(options),
      net::MutableNetworkTrafficAnnotationTag(GetNetworkTrafficAnnotationTag()),
      socket_.BindNewPipeAndPassReceiver(), mojo::NullRemote() /* observer */,
      base::BindOnce(&CastSocketImpl::OnUpgradeToTLS,
                     weak_factory_.GetWeakPtr()));

  return net::ERR_IO_PENDING;
}

int CastSocketImpl::DoSslConnectComplete(int result) {
  logger_->LogSocketEventWithRv(
      channel_id_, ChannelEvent::SSL_SOCKET_CONNECT_COMPLETE, result);
  VLOG_WITH_CONNECTION(1) << "DoSslConnectComplete: " << result;
  if (result == net::OK) {
    if (!peer_cert_) {
      LOG_WITH_CONNECTION(WARNING) << "Could not extract peer cert.";
      SetConnectState(ConnectionState::FINISHED);
      SetErrorState(ChannelError::AUTHENTICATION_ERROR);
      return net::ERR_CERT_INVALID;
    }

    // SSL connection succeeded.
    if (!transport_) {
      // Create a channel transport if one wasn't already set (e.g. by test
      // code).
      transport_ = std::make_unique<CastTransportImpl>(
          mojo_data_pump_.get(), channel_id_, open_params_.ip_endpoint,
          logger_);
    }
    auth_delegate_ = new AuthTransportDelegate(this);
    transport_->SetReadDelegate(base::WrapUnique(auth_delegate_.get()));
    SetConnectState(ConnectionState::AUTH_CHALLENGE_SEND);
  } else if (result == net::ERR_CONNECTION_TIMED_OUT) {
    SetConnectState(ConnectionState::FINISHED);
    SetErrorState(ChannelError::CONNECT_TIMEOUT);
  } else {
    SetConnectState(ConnectionState::FINISHED);
    SetErrorState(ChannelError::AUTHENTICATION_ERROR);
  }
  return result;
}

int CastSocketImpl::DoAuthChallengeSend() {
  VLOG_WITH_CONNECTION(1) << "DoAuthChallengeSend";
  SetConnectState(ConnectionState::AUTH_CHALLENGE_SEND_COMPLETE);

  CastMessage challenge_message;
  CreateAuthChallengeMessage(&challenge_message, auth_context_);
  VLOG_WITH_CONNECTION(1) << "Sending challenge: " << challenge_message;

  ResetConnectLoopCallback();

  transport_->SendMessage(challenge_message, connect_loop_callback_.callback());

  // Always return IO_PENDING since the result is always asynchronous.
  return net::ERR_IO_PENDING;
}

int CastSocketImpl::DoAuthChallengeSendComplete(int result) {
  VLOG_WITH_CONNECTION(1) << "DoAuthChallengeSendComplete: " << result;
  if (result < 0) {
    SetConnectState(ConnectionState::CONNECT_ERROR);
    SetErrorState(ChannelError::CAST_SOCKET_ERROR);
    logger_->LogSocketEventWithRv(
        channel_id_, ChannelEvent::SEND_AUTH_CHALLENGE_FAILED, result);
    return result;
  }
  transport_->Start();
  SetConnectState(ConnectionState::AUTH_CHALLENGE_REPLY_COMPLETE);
  return net::ERR_IO_PENDING;
}

CastSocketImpl::AuthTransportDelegate::AuthTransportDelegate(
    CastSocketImpl* socket)
    : socket_(socket), error_state_(ChannelError::NONE) {
  DCHECK(socket);
}

ChannelError CastSocketImpl::AuthTransportDelegate::error_state() const {
  return error_state_;
}

LastError CastSocketImpl::AuthTransportDelegate::last_error() const {
  return last_error_;
}

void CastSocketImpl::AuthTransportDelegate::OnError(ChannelError error_state) {
  error_state_ = error_state;
  socket_->PostTaskToStartConnectLoop(net::ERR_CONNECTION_FAILED);
}

void CastSocketImpl::AuthTransportDelegate::OnMessage(
    const CastMessage& message) {
  if (!IsAuthMessage(message)) {
    error_state_ = ChannelError::TRANSPORT_ERROR;
    socket_->PostTaskToStartConnectLoop(net::ERR_INVALID_RESPONSE);
  } else {
    socket_->challenge_reply_ = std::make_unique<CastMessage>(message);
    socket_->PostTaskToStartConnectLoop(net::OK);
  }
}

void CastSocketImpl::AuthTransportDelegate::Start() {}

int CastSocketImpl::DoAuthChallengeReplyComplete(int result) {
  VLOG_WITH_CONNECTION(1) << "DoAuthChallengeReplyComplete: " << result;

  if (auth_delegate_->error_state() != ChannelError::NONE) {
    SetErrorState(auth_delegate_->error_state());
    SetConnectState(ConnectionState::CONNECT_ERROR);
    return net::ERR_CONNECTION_FAILED;
  }
  auth_delegate_ = nullptr;

  if (result < 0) {
    SetConnectState(ConnectionState::CONNECT_ERROR);
    return result;
  }

  if (!VerifyChallengeReply()) {
    SetErrorState(ChannelError::AUTHENTICATION_ERROR);
    SetConnectState(ConnectionState::CONNECT_ERROR);
    return net::ERR_CONNECTION_FAILED;
  }
  VLOG_WITH_CONNECTION(1) << "Auth challenge verification succeeded";

  SetConnectState(ConnectionState::FINISHED);
  return net::OK;
}

void CastSocketImpl::OnConnect(
    int result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DoConnectLoop(result);
}

void CastSocketImpl::OnUpgradeToTLS(
    int result,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    const std::optional<net::SSLInfo>& ssl_info) {
  if (result == net::OK) {
    mojo_data_pump_ = std::make_unique<MojoDataPump>(std::move(receive_stream),
                                                     std::move(send_stream));
  }
  if (ssl_info.has_value() && ssl_info->cert)
    peer_cert_ = ssl_info->cert;
  DoConnectLoop(result);
}

void CastSocketImpl::DoConnectCallback() {
  VLOG(1) << "DoConnectCallback (error_state = "
          << ChannelErrorToString(error_state_) << ")";
  if (connect_callbacks_.empty()) {
    DLOG(FATAL) << "Connection callback invoked multiple times.";
    return;
  }

  if (error_state_ == ChannelError::NONE) {
    SetReadyState(ReadyState::OPEN);
    if (keep_alive()) {
      auto* keep_alive_delegate = new KeepAliveDelegate(
          this, logger_, std::move(delegate_), open_params_.ping_interval,
          open_params_.liveness_timeout);
      delegate_.reset(keep_alive_delegate);
    }
    transport_->SetReadDelegate(std::move(delegate_));
  } else {
    CloseInternal();
  }

  for (auto& connect_callback : connect_callbacks_)
    std::move(connect_callback).Run(this);
  connect_callbacks_.clear();
}

void CastSocketImpl::Close(net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CloseInternal();
  // Run this callback last.  It may delete the socket.
  std::move(callback).Run(net::OK);
}

void CastSocketImpl::CloseInternal() {
  // TODO(mfoltz): Enforce this when CastChannelAPITest is rewritten to create
  // and free sockets on the same thread.  crbug.com/398242
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (ready_state_ == ReadyState::CLOSED) {
    return;
  }

  VLOG_WITH_CONNECTION(1) << "Close ReadyState = "
                          << ReadyStateToString(ready_state_);
  observers_.Clear();
  delegate_.reset();
  transport_.reset();
  mojo_data_pump_.reset();
  socket_.reset();
  tcp_socket_.reset();
  if (GetTimer()) {
    GetTimer()->Stop();
  }

  // Cancel callbacks that we queued ourselves to re-enter the connect or read
  // loops.
  connect_loop_callback_.Cancel();
  connect_timeout_callback_.Cancel();
  SetReadyState(ReadyState::CLOSED);
}

base::OneShotTimer* CastSocketImpl::GetTimer() {
  return connect_timeout_timer_.get();
}

void CastSocketImpl::SetConnectState(ConnectionState connect_state) {
  if (connect_state_ != connect_state) {
    connect_state_ = connect_state;
  }
}

void CastSocketImpl::SetReadyState(ReadyState ready_state) {
  if (ready_state_ != ready_state) {
    ready_state_ = ready_state;
    for (auto& observer : observers_)
      observer.OnReadyStateChanged(*this);
  }
}

void CastSocketImpl::SetErrorState(ChannelError error_state) {
  VLOG_WITH_CONNECTION(1) << "SetErrorState "
                          << ChannelErrorToString(error_state);
  DCHECK_EQ(ChannelError::NONE, error_state_);
  error_state_ = error_state;
  delegate_->OnError(error_state_);
}

CastSocketImpl::CastSocketMessageDelegate::CastSocketMessageDelegate(
    CastSocketImpl* socket)
    : socket_(socket) {
  DCHECK(socket_);
}

CastSocketImpl::CastSocketMessageDelegate::~CastSocketMessageDelegate() {}

// CastTransport::Delegate implementation.
void CastSocketImpl::CastSocketMessageDelegate::OnError(
    ChannelError error_state) {
  for (auto& observer : socket_->observers_)
    observer.OnError(*socket_, error_state);
}

void CastSocketImpl::CastSocketMessageDelegate::OnMessage(
    const CastMessage& message) {
  for (auto& observer : socket_->observers_)
    observer.OnMessage(*socket_, message);
}

void CastSocketImpl::CastSocketMessageDelegate::Start() {}

CastSocketOpenParams::CastSocketOpenParams(const net::IPEndPoint& ip_endpoint,
                                           base::TimeDelta connect_timeout)
    : ip_endpoint(ip_endpoint), connect_timeout(connect_timeout) {}

CastSocketOpenParams::CastSocketOpenParams(
    const net::IPEndPoint& ip_endpoint,
    base::TimeDelta connect_timeout,
    base::TimeDelta liveness_timeout,
    base::TimeDelta ping_interval,
    CastDeviceCapabilitySet device_capabilities)
    : ip_endpoint(ip_endpoint),
      connect_timeout(connect_timeout),
      liveness_timeout(liveness_timeout),
      ping_interval(ping_interval),
      device_capabilities(device_capabilities) {}

}  // namespace cast_channel
#undef VLOG_WITH_CONNECTION
