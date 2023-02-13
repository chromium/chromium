// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/libcast_socket_service.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/cast_transport.h"
#include "components/media_router/common/providers/cast/channel/keep_alive_handler.h"
#include "components/media_router/common/providers/cast/channel/logger.h"
#include "components/openscreen_platform/network_context.h"
#include "components/openscreen_platform/network_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/openscreen/src/platform/api/serial_delete_ptr.h"

using content::BrowserThread;

using openscreen::cast::SenderSocketFactory;

namespace cast_channel {
namespace {

ChannelError MapToChannelError(const openscreen::Error& error) {
  switch (error.code()) {
    case openscreen::Error::Code::kCastV2ChannelNotOpen:
      return ChannelError::CHANNEL_NOT_OPEN;
    case openscreen::Error::Code::kCastV2AuthenticationError:
      return ChannelError::AUTHENTICATION_ERROR;
    case openscreen::Error::Code::kCastV2ConnectError:
      return ChannelError::CONNECT_ERROR;
    case openscreen::Error::Code::kCastV2CastSocketError:
      return ChannelError::CAST_SOCKET_ERROR;
    case openscreen::Error::Code::kCastV2TransportError:
      return ChannelError::TRANSPORT_ERROR;
    case openscreen::Error::Code::kCastV2InvalidMessage:
      return ChannelError::INVALID_MESSAGE;
    case openscreen::Error::Code::kCastV2InvalidChannelId:
      return ChannelError::INVALID_CHANNEL_ID;
    case openscreen::Error::Code::kCastV2ConnectTimeout:
      return ChannelError::CONNECT_TIMEOUT;
    case openscreen::Error::Code::kCastV2PingTimeout:
      return ChannelError::PING_TIMEOUT;
    default:
      return ChannelError::UNKNOWN;
  }
}

}  // namespace

LibcastSocketService::ConnectTimer::ConnectTimer(
    std::unique_ptr<base::CancelableOnceClosure> callback,
    std::unique_ptr<base::OneShotTimer> timer)
    : callback(std::move(callback)), timer(std::move(timer)) {}

LibcastSocketService::ConnectTimer::ConnectTimer(ConnectTimer&&) = default;

LibcastSocketService::ConnectTimer::~ConnectTimer() = default;

LibcastSocketService::ConnectTimer&
LibcastSocketService::ConnectTimer::operator=(ConnectTimer&&) = default;

int LibcastSocketService::last_channel_id_ = 0;

class CastSocketWrapper final : public CastSocket {
 public:
  class Transport final : public CastTransport {
   public:
    Transport(LibcastSocket* socket,
              openscreen_platform::TaskRunner* openscreen_task_runner,
              scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
        : socket_(socket),
          openscreen_task_runner_(openscreen_task_runner),
          io_task_runner_(std::move(io_task_runner)) {}

    ~Transport() override = default;

    // CastTransport overrides.
    void SendMessage(const CastMessage& message,
                     net::CompletionOnceCallback callback) override {
      DVLOG(1) << "sending message on socket " << socket_->socket_id();
      DVLOG_IF(1, message.payload_type() ==
                      ::cast::channel::CastMessage_PayloadType_STRING)
          << message;
      openscreen_task_runner_->PostTask(
          [this, message, c = std::move(callback)]() mutable {
            openscreen::Error error = socket_->Send(message);
            int result = error.ok() ? net::OK : net::ERR_FAILED;
            io_task_runner_->PostTask(FROM_HERE,
                                      base::BindOnce(std::move(c), result));
          });
    }

    void Start() override { NOTREACHED(); }
    void SetReadDelegate(std::unique_ptr<Delegate> delegate) override {
      NOTREACHED();
    }

    raw_ptr<LibcastSocket> socket_;
    raw_ptr<openscreen_platform::TaskRunner> openscreen_task_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  };

  CastSocketWrapper(std::unique_ptr<LibcastSocket> socket,
                    const openscreen::IPEndpoint& endpoint,
                    openscreen_platform::TaskRunner* openscreen_task_runner,
                    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : socket_(openscreen_task_runner, socket.release()),
        endpoint_(openscreen_platform::ToNetEndPoint(endpoint)),
        transport_(socket_.get(),
                   openscreen_task_runner,
                   std::move(io_task_runner)) {}

  ~CastSocketWrapper() override = default;

  // CastSocket overrides.
  void Connect(OnOpenCallback callback) override { NOTREACHED(); }
  void Close(net::CompletionOnceCallback callback) override {
    ready_state_ = ReadyState::CLOSED;
    std::move(callback).Run(net::OK);
  }

  const net::IPEndPoint& ip_endpoint() const override { return endpoint_; }

  int id() const override { return socket_->socket_id(); }

  void set_id(int id) override { NOTREACHED(); }

  ReadyState ready_state() const override { return ready_state_; }

  ChannelError error_state() const override { return error_state_; }

  CastChannelFlags flags() const override { return flags_; }

  bool keep_alive() const override { return !!keep_alive_handler_; }

  bool audio_only() const override { return socket_->audio_only(); }

  void SetErrorState(ChannelError error_state) override {
    error_state_ = error_state;
  }

  CastTransport* transport() const override {
    return const_cast<CastTransport*>(
        static_cast<const CastTransport*>(&transport_));
  }

  void AddObserver(Observer* observer) override { NOTREACHED(); }
  void RemoveObserver(Observer* observer) override { NOTREACHED(); }

 private:
  friend class LibcastSocketService;

  openscreen::SerialDeletePtr<LibcastSocket> socket_;
  net::IPEndPoint endpoint_;
  Transport transport_;
  ReadyState ready_state_ = ReadyState::OPEN;
  ChannelError error_state_ = ChannelError::NONE;
  CastChannelFlags flags_ = 0;
  std::unique_ptr<KeepAliveHandler> keep_alive_handler_;
};

LibcastSocketService::LibcastSocketService()
    : openscreen_task_runner_(
          // NOTE: Network service must be accessed on UI thread.
          content::GetUIThreadTaskRunner({})),
      socket_factory_(this, &openscreen_task_runner_),
      tls_factory_(openscreen::TlsConnectionFactory::CreateFactory(
          &socket_factory_,
          &openscreen_task_runner_)) {
  socket_factory_.set_factory(tls_factory_.get());
}

// This is a leaky singleton and the dtor won't be called.
LibcastSocketService::~LibcastSocketService() = default;

std::unique_ptr<CastSocket> LibcastSocketService::RemoveSocket(int channel_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(channel_id > 0);
  auto socket_it = sockets_.find(channel_id);

  std::unique_ptr<CastSocket> socket;
  if (socket_it != sockets_.end()) {
    socket = std::move(socket_it->second);
    sockets_.erase(socket_it);
    for (auto entry = socket_endpoints_.begin();
         entry != socket_endpoints_.end(); ++entry) {
      if (entry->second == channel_id) {
        socket_endpoints_.erase(entry);
        break;
      }
    }
  }
  return socket;
}

CastSocket* LibcastSocketService::GetSocket(int channel_id) const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(channel_id > 0);
  const auto& socket_it = sockets_.find(channel_id);
  return socket_it == sockets_.end() ? nullptr : socket_it->second.get();
}

CastSocket* LibcastSocketService::GetSocket(
    const net::IPEndPoint& ip_endpoint) const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto it = base::ranges::find(sockets_, ip_endpoint,
                               [](const Sockets::value_type& pair) {
                                 return pair.second->ip_endpoint();
                               });
  return it == sockets_.end() ? nullptr : it->second.get();
}

void LibcastSocketService::OpenSocket(
    NetworkContextGetter network_context_getter,
    const CastSocketOpenParams& open_params,
    CastSocket::OnOpenCallback open_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!socket_for_test_);

  const net::IPEndPoint& ip_endpoint = open_params.ip_endpoint;
  CastSocket* socket = GetSocket(ip_endpoint);
  if (socket) {
    std::move(open_cb).Run(socket);
  } else {
    bool pending = EndpointPending(ip_endpoint);
    openscreen::IPEndpoint remote =
        openscreen_platform::ToOpenScreenEndPoint(open_params.ip_endpoint);
    if (!pending) {
      std::unique_ptr<base::CancelableOnceClosure> connect_timeout_callback;
      std::unique_ptr<base::OneShotTimer> connect_timer;
      if (open_params.connect_timeout.is_positive()) {
        connect_timeout_callback =
            std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
                &LibcastSocketService::OnErrorIOThread, base::Unretained(this),
                base::Unretained(&socket_factory_), remote,
                openscreen::Error::Code::kCastV2ConnectTimeout));
        connect_timer = std::make_unique<base::OneShotTimer>();
        connect_timer->Start(FROM_HERE, open_params.connect_timeout,
                             connect_timeout_callback->callback());
      }
      pending_endpoints_.emplace(
          remote, ConnectTimer(std::move(connect_timeout_callback),
                               std::move(connect_timer)));
      if (libcast_socket_for_test_) {
        libcast_socket_for_test_->SetClient(this);
        task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&LibcastSocketService::OnConnectedIOThread,
                           base::Unretained(this),
                           base::Unretained(&socket_factory_), remote,
                           std::move(libcast_socket_for_test_)));
      } else {
        openscreen_task_runner_.task_runner()->PostTask(
            FROM_HERE,
            base::BindOnce(&SenderSocketFactory::Connect,
                           base::Unretained(&socket_factory_), remote,
                           SenderSocketFactory::DeviceMediaPolicy::kNone,
                           this));
      }
      open_params_[remote] = {open_params.ping_interval,
                              open_params.liveness_timeout};
    }

    open_callbacks_[remote].emplace_back(std::move(open_cb));
  }
}

void LibcastSocketService::AddObserver(CastSocket::Observer* observer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(observer);
  if (observers_.HasObserver(observer))
    return;

  observers_.AddObserver(observer);
}

void LibcastSocketService::RemoveObserver(CastSocket::Observer* observer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(observer);

  observers_.RemoveObserver(observer);
}

void LibcastSocketService::OnError(LibcastSocket* socket,
                                   openscreen::Error error) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LibcastSocketService::OnErrorSocketIOThread,
                     base::Unretained(this), socket, std::move(error)));
}

void LibcastSocketService::OnMessage(LibcastSocket* socket,
                                     ::cast::channel::CastMessage message) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LibcastSocketService::OnMessageIOThread,
                     base::Unretained(this), socket, std::move(message)));
}

void LibcastSocketService::OnConnected(SenderSocketFactory* factory,
                                       const openscreen::IPEndpoint& endpoint,
                                       std::unique_ptr<LibcastSocket> socket) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LibcastSocketService::OnConnectedIOThread,
                                base::Unretained(this), factory, endpoint,
                                std::move(socket)));
}

bool LibcastSocketService::EndpointPending(
    const net::IPEndPoint& ip_endpoint) const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto endpoint = openscreen_platform::ToOpenScreenEndPoint(ip_endpoint);
  auto entry = pending_endpoints_.find(endpoint);
  return entry != pending_endpoints_.end();
}

void LibcastSocketService::OnErrorSocketIOThread(LibcastSocket* socket,
                                                 openscreen::Error error) {
  auto entry = sockets_.find(socket->socket_id());
  if (entry != sockets_.end()) {
    ChannelError channel_error = MapToChannelError(error);
    DCHECK_NE(channel_error, ChannelError::NONE) << error;
    entry->second->SetErrorState(channel_error);
    for (auto& observer : observers_) {
      observer.OnError(*entry->second, channel_error);
    }
  }
}

void LibcastSocketService::OnMessageIOThread(
    LibcastSocket* socket,
    ::cast::channel::CastMessage message) {
  auto entry = sockets_.find(socket->socket_id());
  if (entry != sockets_.end()) {
    if (!entry->second->keep_alive_handler_ ||
        !entry->second->keep_alive_handler_->HandleMessage(message)) {
      for (auto& observer : observers_) {
        observer.OnMessage(*entry->second, message);
      }
    }
  }
}

void LibcastSocketService::OnError(SenderSocketFactory* factory,
                                   const openscreen::IPEndpoint& endpoint,
                                   openscreen::Error error) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LibcastSocketService::OnErrorIOThread,
                                base::Unretained(this), factory, endpoint,
                                std::move(error)));
}

void LibcastSocketService::OnConnectedIOThread(
    SenderSocketFactory* factory,
    const openscreen::IPEndpoint& endpoint,
    std::unique_ptr<LibcastSocket> socket) {
  auto entry = pending_endpoints_.find(endpoint);
  if (entry == pending_endpoints_.end()) {
    return;
  }
  if (entry->second.timer) {
    entry->second.callback->Cancel();
    entry->second.timer->Stop();
  }

  auto params = open_params_.find(endpoint);
  DCHECK(params != open_params_.end());
  auto* socket_ptr = socket.get();
  auto socket_wrapper = std::make_unique<CastSocketWrapper>(
      std::move(socket), endpoint, &openscreen_task_runner_, task_runner_);
  if (params->second.liveness_timeout.is_positive()) {
    auto keep_alive_handler = std::make_unique<KeepAliveHandler>(
        socket_wrapper.get(), logger_, params->second.ping_interval,
        params->second.liveness_timeout,
        base::BindRepeating(&LibcastSocketService::OnErrorBounce,
                            base::Unretained(this),
                            base::Unretained(socket_ptr)));
    keep_alive_handler->Start();
    socket_wrapper->keep_alive_handler_ = std::move(keep_alive_handler);
  }
  auto result =
      sockets_.emplace(socket_ptr->socket_id(), std::move(socket_wrapper));
  socket_endpoints_.emplace(endpoint, socket_ptr->socket_id());
  pending_endpoints_.erase(entry);
  open_params_.erase(params);

  auto callbacks = open_callbacks_.find(endpoint);
  if (callbacks != open_callbacks_.end()) {
    for (auto& cb : callbacks->second) {
      std::move(cb).Run(result.first->second.get());
    }
    open_callbacks_.erase(callbacks);
  }
}

void LibcastSocketService::OnErrorIOThread(
    SenderSocketFactory* factory,
    const openscreen::IPEndpoint& endpoint,
    openscreen::Error error) {
  auto socket_wrapper = std::make_unique<CastSocketWrapper>(
      nullptr, endpoint, &openscreen_task_runner_, task_runner_);
  socket_wrapper->Close(base::DoNothing());
  socket_wrapper->SetErrorState(MapToChannelError(error));
  pending_endpoints_.erase(endpoint);

  int error_channel_id = --last_channel_id_;
  auto result = sockets_.emplace(error_channel_id, std::move(socket_wrapper));
  socket_endpoints_.emplace(endpoint, error_channel_id);

  auto callbacks = open_callbacks_.find(endpoint);
  if (callbacks != open_callbacks_.end()) {
    for (auto& cb : callbacks->second) {
      std::move(cb).Run(result.first->second.get());
    }
    open_callbacks_.erase(callbacks);
  }
}

void LibcastSocketService::OnErrorBounce(LibcastSocket* socket,
                                         ChannelError error) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LibcastSocketService::OnErrorSocketIOThread, base::Unretained(this),
          socket,
          openscreen::Error(openscreen::Error::Code::kSocketClosedFailure)));
}

}  // namespace cast_channel
