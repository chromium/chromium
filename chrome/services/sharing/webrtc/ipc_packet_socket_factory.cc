// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/sharing/webrtc/ipc_packet_socket_factory.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <optional>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "chrome/services/sharing/webrtc/p2p_async_address_resolver.h"
#include "chrome/services/sharing/webrtc/p2p_socket_client.h"
#include "chrome/services/sharing/webrtc/p2p_socket_client_delegate.h"
#include "components/webrtc/net_address_utils.h"
#include "net/base/ip_address.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"
#include "third_party/webrtc/rtc_base/network/received_packet.h"

namespace sharing {

namespace {

const int kDefaultNonSetOptionValue = -1;

bool IsTcpClientSocket(network::P2PSocketType type) {
  return (type == network::P2P_SOCKET_STUN_TCP_CLIENT) ||
         (type == network::P2P_SOCKET_TCP_CLIENT) ||
         (type == network::P2P_SOCKET_STUN_SSLTCP_CLIENT) ||
         (type == network::P2P_SOCKET_SSLTCP_CLIENT) ||
         (type == network::P2P_SOCKET_TLS_CLIENT) ||
         (type == network::P2P_SOCKET_STUN_TLS_CLIENT);
}

bool JingleSocketOptionToP2PSocketOption(rtc::Socket::Option option,
                                         network::P2PSocketOption* ipc_option) {
  switch (option) {
    case rtc::Socket::OPT_RCVBUF:
      *ipc_option = network::P2P_SOCKET_OPT_RCVBUF;
      break;
    case rtc::Socket::OPT_SNDBUF:
      *ipc_option = network::P2P_SOCKET_OPT_SNDBUF;
      break;
    case rtc::Socket::OPT_DSCP:
      *ipc_option = network::P2P_SOCKET_OPT_DSCP;
      break;
    case rtc::Socket::OPT_RECV_ECN:
      *ipc_option = network::P2P_SOCKET_OPT_RECV_ECN;
      break;
    case rtc::Socket::OPT_DONTFRAGMENT:
    case rtc::Socket::OPT_NODELAY:
    case rtc::Socket::OPT_IPV6_V6ONLY:
    case rtc::Socket::OPT_RTP_SENDTIME_EXTN_ID:
      return false;  // Not supported by the chrome sockets.
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
  return true;
}

const size_t kMaximumInFlightBytes = 64 * 1024;  // 64 KB

// IpcPacketSocket implements rtc::AsyncPacketSocket interface
// using P2PSocketClient that works over IPC-channel. It must be used
// on the thread it was created.
class IpcPacketSocket : public rtc::AsyncPacketSocket,
                        public P2PSocketClientDelegate {
 public:
  IpcPacketSocket();
  IpcPacketSocket(const IpcPacketSocket&) = delete;
  IpcPacketSocket& operator=(const IpcPacketSocket&) = delete;
  ~IpcPacketSocket() override;

  // Struct to track information when a packet is received by this socket for
  // send. The information tracked here will be used to match with the
  // P2PSendPacketMetrics from the underneath system socket.
  struct InFlightPacketRecord {
    InFlightPacketRecord(uint64_t packet_id, size_t packet_size)
        : packet_id(packet_id), packet_size(packet_size) {}

    uint64_t packet_id;
    size_t packet_size;
  };

  typedef std::list<InFlightPacketRecord> InFlightPacketList;

  // Always takes ownership of client even if initialization fails.
  bool Init(network::P2PSocketType type,
            std::unique_ptr<P2PSocketClient> client,
            const rtc::SocketAddress& local_address,
            uint16_t min_port,
            uint16_t max_port,
            const rtc::SocketAddress& remote_address);

  // rtc::AsyncPacketSocket interface.
  rtc::SocketAddress GetLocalAddress() const override;
  rtc::SocketAddress GetRemoteAddress() const override;
  int Send(const void* pv,
           size_t cb,
           const rtc::PacketOptions& options) override;
  int SendTo(const void* pv,
             size_t cb,
             const rtc::SocketAddress& addr,
             const rtc::PacketOptions& options) override;
  int Close() override;
  State GetState() const override;
  int GetOption(rtc::Socket::Option option, int* value) override;
  int SetOption(rtc::Socket::Option option, int value) override;
  int GetError() const override;
  void SetError(int error) override;

  // P2PSocketClientDelegate implementation.
  void OnOpen(const net::IPEndPoint& local_address,
              const net::IPEndPoint& remote_address) override;
  void OnSendComplete(
      const network::P2PSendPacketMetrics& send_metrics) override;
  void OnError() override;
  void OnDataReceived(const net::IPEndPoint& address,
                      base::span<const uint8_t> data,
                      const base::TimeTicks& timestamp) override;

 private:
  enum InternalState {
    IS_UNINITIALIZED,
    IS_OPENING,
    IS_OPEN,
    IS_CLOSED,
    IS_ERROR,
  };

  // Increment the counter for consecutive bytes discarded as socket is running
  // out of buffer.
  void IncrementDiscardCounters(size_t bytes_discarded);

  void InitAcceptedTcp(std::unique_ptr<P2PSocketClient> client,
                       const rtc::SocketAddress& local_address,
                       const rtc::SocketAddress& remote_address);

  int DoSetOption(network::P2PSocketOption option, int value);

  network::P2PSocketType type_;

  // Used to verify that a method runs on the thread that created this socket.
  THREAD_CHECKER(thread_checker_);

  // Corresponding P2P socket client.
  std::unique_ptr<P2PSocketClient> client_;

  // Local address is allocated by the browser process, and the
  // renderer side doesn't know the address until it receives OnOpen()
  // event from the browser.
  rtc::SocketAddress local_address_;

  // Remote address for client TCP connections.
  rtc::SocketAddress remote_address_;

  // Current state of the object.
  InternalState state_;

  // Track the number of bytes allowed to be sent non-blocking. This is used to
  // throttle the sending of packets to the browser process. For each packet
  // sent, the value is decreased. As callbacks to OnSendComplete() (as IPCs
  // from the browser process) are made, the value is increased back. This
  // allows short bursts of high-rate sending without dropping packets, but
  // quickly restricts the client to a sustainable steady-state rate.
  size_t send_bytes_available_;

  // Used to detect when browser doesn't send SendComplete message for some
  // packets. In normal case, the first packet should be the one that we're
  // going to receive the next completion signal.
  InFlightPacketList in_flight_packet_records_;

  // Set to true once EWOULDBLOCK was returned from Send(). Indicates that the
  // caller expects SignalWritable notification.
  bool writable_signal_expected_;

  // Current error code. Valid when state_ == IS_ERROR.
  int error_;
  int options_[network::P2P_SOCKET_OPT_MAX];

  // Track the maximum and current consecutive bytes discarded due to not enough
  // send_bytes_available_.
  size_t max_discard_bytes_sequence_;
  size_t current_discard_bytes_sequence_;

  // Track the total number of packets and the number of packets discarded.
  size_t packets_discarded_;
  size_t total_packets_;
};

// Simple wrapper around P2PAsyncAddressResolver. The main purpose of this
// class is to send SignalDone, after OnDone callback from
// P2PAsyncAddressResolver. Libjingle sig slots are not thread safe. In case
// of MT sig slots clients must call disconnect. This class is to make sure
// we destruct from the same thread on which is created.
class AsyncDnsAddressResolverImpl : public webrtc::AsyncDnsResolverInterface,
                                    webrtc::AsyncDnsResolverResult {
 public:
  explicit AsyncDnsAddressResolverImpl(
      const mojo::SharedRemote<network::mojom::P2PSocketManager>&
          socket_manager);
  ~AsyncDnsAddressResolverImpl() override;

  // rtc::AsyncDnsResolverInterface interface.
  void Start(const rtc::SocketAddress& addr,
             absl::AnyInvocable<void()> callback) override;
  void Start(const rtc::SocketAddress& addr,
             int family,
             absl::AnyInvocable<void()> callback) override;
  const AsyncDnsResolverResult& result() const override { return *this; }

  // webrtc::AsyncDnsResolverResult interface
  bool GetResolvedAddress(int family, rtc::SocketAddress* addr) const override;
  int GetError() const override;

 private:
  virtual void OnAddressResolved(const std::vector<net::IPAddress>& addresses);

  P2PAsyncAddressResolver resolver_;

  SEQUENCE_CHECKER(sequence_checker_);

  rtc::SocketAddress addr_;                // Address to resolve.
  absl::AnyInvocable<void()> callback_;
  std::vector<rtc::IPAddress> addresses_;  // Resolved addresses.

  base::WeakPtrFactory<AsyncDnsAddressResolverImpl> weak_factory_{this};
};

IpcPacketSocket::IpcPacketSocket()
    : type_(network::P2P_SOCKET_UDP),
      state_(IS_UNINITIALIZED),
      send_bytes_available_(kMaximumInFlightBytes),
      writable_signal_expected_(false),
      error_(0),
      max_discard_bytes_sequence_(0),
      current_discard_bytes_sequence_(0),
      packets_discarded_(0),
      total_packets_(0) {
  static_assert(kMaximumInFlightBytes > 0, "would send at zero rate");
  std::fill_n(options_, static_cast<int>(network::P2P_SOCKET_OPT_MAX),
              kDefaultNonSetOptionValue);
}

IpcPacketSocket::~IpcPacketSocket() {
  if (state_ == IS_OPENING || state_ == IS_OPEN || state_ == IS_ERROR) {
    Close();
  }
}

void IpcPacketSocket::IncrementDiscardCounters(size_t bytes_discarded) {
  current_discard_bytes_sequence_ += bytes_discarded;
  packets_discarded_++;

  if (current_discard_bytes_sequence_ > max_discard_bytes_sequence_) {
    max_discard_bytes_sequence_ = current_discard_bytes_sequence_;
  }
}

bool IpcPacketSocket::Init(network::P2PSocketType type,
                           std::unique_ptr<P2PSocketClient> client,
                           const rtc::SocketAddress& local_address,
                           uint16_t min_port,
                           uint16_t max_port,
                           const rtc::SocketAddress& remote_address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, IS_UNINITIALIZED);

  type_ = type;
  auto* client_ptr = client.get();
  client_ = std::move(client);
  local_address_ = local_address;
  remote_address_ = remote_address;
  state_ = IS_OPENING;

  net::IPEndPoint local_endpoint;
  if (!webrtc::SocketAddressToIPEndPoint(local_address, &local_endpoint)) {
    return false;
  }

  net::IPEndPoint remote_endpoint;
  if (!remote_address.IsNil()) {
    DCHECK(IsTcpClientSocket(type_));

    if (remote_address.IsUnresolvedIP()) {
      remote_endpoint =
          net::IPEndPoint(net::IPAddress(), remote_address.port());
    } else {
      if (!webrtc::SocketAddressToIPEndPoint(remote_address,
                                             &remote_endpoint)) {
        return false;
      }
    }
  }

  // We need to send both resolved and unresolved address in Init. Unresolved
  // address will be used in case of TLS for certificate hostname matching.
  // Certificate will be tied to domain name not to IP address.
  network::P2PHostAndIPEndPoint remote_info(remote_address.hostname(),
                                            remote_endpoint);

  client_ptr->Init(type, local_endpoint, min_port, max_port, remote_info, this);

  return true;
}

void IpcPacketSocket::InitAcceptedTcp(
    std::unique_ptr<P2PSocketClient> client,
    const rtc::SocketAddress& local_address,
    const rtc::SocketAddress& remote_address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, IS_UNINITIALIZED);

  client_ = std::move(client);
  local_address_ = local_address;
  remote_address_ = remote_address;
  state_ = IS_OPEN;
  client_->SetDelegate(this);
}

// rtc::AsyncPacketSocket interface.
rtc::SocketAddress IpcPacketSocket::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return local_address_;
}

rtc::SocketAddress IpcPacketSocket::GetRemoteAddress() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return remote_address_;
}

int IpcPacketSocket::Send(const void* data,
                          size_t data_size,
                          const rtc::PacketOptions& options) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return SendTo(data, data_size, remote_address_, options);
}

int IpcPacketSocket::SendTo(const void* data,
                            size_t data_size,
                            const rtc::SocketAddress& address,
                            const rtc::PacketOptions& options) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  switch (state_) {
    case IS_UNINITIALIZED:
      NOTREACHED_IN_MIGRATION();
      error_ = EWOULDBLOCK;
      return -1;
    case IS_OPENING:
      error_ = EWOULDBLOCK;
      return -1;
    case IS_CLOSED:
      error_ = ENOTCONN;
      return -1;
    case IS_ERROR:
      return -1;
    case IS_OPEN:
      // Continue sending the packet.
      break;
  }

  if (data_size == 0) {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }

  total_packets_++;

  if (data_size > send_bytes_available_) {
    if (!writable_signal_expected_)
      writable_signal_expected_ = true;

    error_ = EWOULDBLOCK;
    IncrementDiscardCounters(data_size);
    return -1;
  } else {
    current_discard_bytes_sequence_ = 0;
  }

  net::IPEndPoint address_chrome;
  if (address.IsUnresolvedIP()) {
    address_chrome = net::IPEndPoint(net::IPAddress(), address.port());
  } else {
    if (!webrtc::SocketAddressToIPEndPoint(address, &address_chrome)) {
      LOG(WARNING) << "Failed to convert remote address to IPEndPoint: address="
                   << address.ipaddr().ToSensitiveString()
                   << ", remote_address_="
                   << remote_address_.ipaddr().ToSensitiveString();
      NOTREACHED_IN_MIGRATION();
      error_ = EINVAL;
      return -1;
    }
  }

  send_bytes_available_ -= data_size;

  uint64_t packet_id = client_->Send(
      address_chrome,
      base::make_span(static_cast<const uint8_t*>(data), data_size), options);

  // Ensure packet_id is not 0. It can't be the case according to
  // P2PSocketClient::Send().
  DCHECK_NE(packet_id, 0uL);

  in_flight_packet_records_.push_back(
      InFlightPacketRecord(packet_id, data_size));

  // Fake successful send. The caller ignores result anyway.
  return data_size;
}

int IpcPacketSocket::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  client_->Close();
  state_ = IS_CLOSED;

  return 0;
}

rtc::AsyncPacketSocket::State IpcPacketSocket::GetState() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  switch (state_) {
    case IS_UNINITIALIZED:
      NOTREACHED_IN_MIGRATION();
      return STATE_CLOSED;

    case IS_OPENING:
      return STATE_BINDING;

    case IS_OPEN:
      if (IsTcpClientSocket(type_)) {
        return STATE_CONNECTED;
      } else {
        return STATE_BOUND;
      }

    case IS_CLOSED:
    case IS_ERROR:
      return STATE_CLOSED;
  }

  NOTREACHED_IN_MIGRATION();
  return STATE_CLOSED;
}

int IpcPacketSocket::GetOption(rtc::Socket::Option option, int* value) {
  network::P2PSocketOption p2p_socket_option = network::P2P_SOCKET_OPT_MAX;
  if (!JingleSocketOptionToP2PSocketOption(option, &p2p_socket_option)) {
    // unsupported option.
    return -1;
  }

  *value = options_[p2p_socket_option];
  return 0;
}

int IpcPacketSocket::SetOption(rtc::Socket::Option option, int value) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  network::P2PSocketOption p2p_socket_option = network::P2P_SOCKET_OPT_MAX;
  if (!JingleSocketOptionToP2PSocketOption(option, &p2p_socket_option)) {
    // Option is not supported.
    return -1;
  }

  options_[p2p_socket_option] = value;

  if (state_ == IS_OPEN) {
    // Options will be applied when state becomes IS_OPEN in OnOpen.
    return DoSetOption(p2p_socket_option, value);
  }
  return 0;
}

int IpcPacketSocket::DoSetOption(network::P2PSocketOption option, int value) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, IS_OPEN);

  client_->SetOption(option, value);
  return 0;
}

int IpcPacketSocket::GetError() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return error_;
}

void IpcPacketSocket::SetError(int error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  error_ = error;
}

void IpcPacketSocket::OnOpen(const net::IPEndPoint& local_address,
                             const net::IPEndPoint& remote_address) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!webrtc::IPEndPointToSocketAddress(local_address, &local_address_)) {
    // Always expect correct IPv4 address to be allocated.
    NOTREACHED_IN_MIGRATION();
    OnError();
    return;
  }

  state_ = IS_OPEN;

  // Set all pending options if any.
  for (int i = 0; i < network::P2P_SOCKET_OPT_MAX; ++i) {
    if (options_[i] != kDefaultNonSetOptionValue)
      DoSetOption(static_cast<network::P2PSocketOption>(i), options_[i]);
  }

  SignalAddressReady(this, local_address_);
  if (IsTcpClientSocket(type_)) {
    // If remote address is unresolved, set resolved remote IP address received
    // in the callback. This address will be used while sending the packets
    // over the network.
    if (remote_address_.IsUnresolvedIP()) {
      rtc::SocketAddress jingle_socket_address;
      // |remote_address| could be unresolved if the connection is behind a
      // proxy.
      if (!remote_address.address().empty() &&
          webrtc::IPEndPointToSocketAddress(remote_address,
                                            &jingle_socket_address)) {
        // Set only the IP address.
        remote_address_.SetResolvedIP(jingle_socket_address.ipaddr());
      }
    }

    // SignalConnect after updating the |remote_address_| so that the listener
    // can get the resolved remote address.
    SignalConnect(this);
  }
}

void IpcPacketSocket::OnSendComplete(
    const network::P2PSendPacketMetrics& send_metrics) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CHECK(!in_flight_packet_records_.empty());

  const InFlightPacketRecord& record = in_flight_packet_records_.front();

  // Tracking is not turned on for TCP so it's always 0. For UDP, this will
  // cause a crash when the packet ids don't match.
  CHECK(send_metrics.packet_id == 0 ||
        record.packet_id == send_metrics.packet_id);

  send_bytes_available_ += record.packet_size;

  DCHECK_LE(send_bytes_available_, kMaximumInFlightBytes);

  in_flight_packet_records_.pop_front();

  SignalSentPacket(this, rtc::SentPacket(send_metrics.rtc_packet_id,
                                         send_metrics.send_time_ms));

  if (writable_signal_expected_ && send_bytes_available_ > 0) {
    // blink::WebRtcLogMessage(base::StringPrintf(
    //    "IpcPacketSocket: sending is unblocked. %d packets in flight.",
    //    static_cast<int>(in_flight_packet_records_.size())));

    writable_signal_expected_ = false;
    SignalReadyToSend(this);
  }
}

void IpcPacketSocket::OnError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool was_closed = (state_ == IS_ERROR || state_ == IS_CLOSED);
  state_ = IS_ERROR;
  error_ = ECONNABORTED;
  if (!was_closed) {
    SignalClose(this, 0);
  }
}

void IpcPacketSocket::OnDataReceived(const net::IPEndPoint& address,
                                     base::span<const uint8_t> data,
                                     const base::TimeTicks& timestamp) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  rtc::SocketAddress address_lj;

  if (address.address().empty()) {
    DCHECK(IsTcpClientSocket(type_));
    // |address| could be empty for TCP connections behind a proxy.
    address_lj = remote_address_;
  } else {
    if (!webrtc::IPEndPointToSocketAddress(address, &address_lj)) {
      // We should always be able to convert address here because we
      // don't expect IPv6 address on IPv4 connections.
      NOTREACHED_IN_MIGRATION();
      return;
    }
  }
  NotifyPacketReceived(rtc::ReceivedPacket(
      data, address_lj,
      webrtc::Timestamp::Micros(timestamp.since_origin().InMicroseconds())));
}

AsyncDnsAddressResolverImpl::AsyncDnsAddressResolverImpl(
    const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager)
    : resolver_(socket_manager) {}

AsyncDnsAddressResolverImpl::~AsyncDnsAddressResolverImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  resolver_.Cancel();
}

void AsyncDnsAddressResolverImpl::Start(const rtc::SocketAddress& addr,
                                        absl::AnyInvocable<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Port and hostname must be copied to the resolved address returned from
  // GetResolvedAddress.
  addr_ = addr;
  callback_ = std::move(callback);
  resolver_.Start(
      addr, std::nullopt,
      base::BindOnce(&AsyncDnsAddressResolverImpl::OnAddressResolved,
                     base::Unretained(this)));
}

void AsyncDnsAddressResolverImpl::Start(const rtc::SocketAddress& addr,
                                        int address_family,
                                        absl::AnyInvocable<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Port and hostname must be copied to the resolved address returned from
  // GetResolvedAddress.
  addr_ = addr;
  callback_ = std::move(callback);
  resolver_.Start(
      addr, address_family,
      base::BindOnce(&AsyncDnsAddressResolverImpl::OnAddressResolved,
                     base::Unretained(this)));
}

bool AsyncDnsAddressResolverImpl::GetResolvedAddress(
    int family,
    rtc::SocketAddress* addr) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (addresses_.empty())
    return false;

  for (const auto& address : addresses_) {
    if (family == address.family()) {
      *addr = addr_;
      addr->SetResolvedIP(address);
      return true;
    }
  }
  return false;
}

int AsyncDnsAddressResolverImpl::GetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return addresses_.empty() ? -1 : 0;
}

void AsyncDnsAddressResolverImpl::OnAddressResolved(
    const std::vector<net::IPAddress>& addresses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& address : addresses) {
    rtc::SocketAddress socket_address;
    if (!webrtc::IPEndPointToSocketAddress(net::IPEndPoint(address, 0),
                                           &socket_address)) {
      NOTREACHED_IN_MIGRATION();
    }
    addresses_.push_back(socket_address.ipaddr());
  }
  callback_();
}

}  // namespace

IpcPacketSocketFactory::IpcPacketSocketFactory(
    const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : socket_manager_(socket_manager), traffic_annotation_(traffic_annotation) {
  DCHECK(socket_manager_.is_bound());
}

IpcPacketSocketFactory::~IpcPacketSocketFactory() = default;

rtc::AsyncPacketSocket* IpcPacketSocketFactory::CreateUdpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port) {
  auto socket_client =
      std::make_unique<P2PSocketClient>(socket_manager_, traffic_annotation_);
  std::unique_ptr<IpcPacketSocket> socket(new IpcPacketSocket());
  if (!socket->Init(network::P2P_SOCKET_UDP, std::move(socket_client),
                    local_address, min_port, max_port, rtc::SocketAddress())) {
    return nullptr;
  }
  return socket.release();
}

rtc::AsyncListenSocket* IpcPacketSocketFactory::CreateServerTcpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port,
    int opts) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

rtc::AsyncPacketSocket* IpcPacketSocketFactory::CreateClientTcpSocket(
    const rtc::SocketAddress& local_address,
    const rtc::SocketAddress& remote_address,
    const rtc::PacketSocketTcpOptions& opts) {
  network::P2PSocketType type;
  if (opts.opts & rtc::PacketSocketFactory::OPT_SSLTCP) {
    type = (opts.opts & rtc::PacketSocketFactory::OPT_STUN)
               ? network::P2P_SOCKET_STUN_SSLTCP_CLIENT
               : network::P2P_SOCKET_SSLTCP_CLIENT;
  } else if (opts.opts & rtc::PacketSocketFactory::OPT_TLS) {
    type = (opts.opts & rtc::PacketSocketFactory::OPT_STUN)
               ? network::P2P_SOCKET_STUN_TLS_CLIENT
               : network::P2P_SOCKET_TLS_CLIENT;
  } else {
    type = (opts.opts & rtc::PacketSocketFactory::OPT_STUN)
               ? network::P2P_SOCKET_STUN_TCP_CLIENT
               : network::P2P_SOCKET_TCP_CLIENT;
  }
  auto socket_client =
      std::make_unique<P2PSocketClient>(socket_manager_, traffic_annotation_);
  std::unique_ptr<IpcPacketSocket> socket(new IpcPacketSocket());
  if (!socket->Init(type, std::move(socket_client), local_address, 0, 0,
                    remote_address))
    return nullptr;
  return socket.release();
}

std::unique_ptr<webrtc::AsyncDnsResolverInterface>
IpcPacketSocketFactory::CreateAsyncDnsResolver() {
  return std::make_unique<AsyncDnsAddressResolverImpl>(socket_manager_);
}

}  // namespace sharing
