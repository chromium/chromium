// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_LIBCAST_SOCKET_SERVICE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_LIBCAST_SOCKET_SERVICE_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"
#include "components/openscreen_platform/task_runner.h"
#include "third_party/openscreen/src/cast/common/public/cast_socket.h"
#include "third_party/openscreen/src/cast/sender/public/sender_socket_factory.h"
#include "third_party/openscreen/src/platform/api/tls_connection_factory.h"

namespace cast_channel {

using LibcastSocket = openscreen::cast::CastSocket;

class CastSocketWrapper;

class LibcastSocketService final
    : public CastSocketService,
      public openscreen::cast::CastSocket::Client,
      public openscreen::cast::SenderSocketFactory::Client {
 public:
  using CastSocketService::NetworkContextGetter;

  LibcastSocketService(const LibcastSocketService&) = delete;
  LibcastSocketService& operator=(const LibcastSocketService&) = delete;

  ~LibcastSocketService() override;

  // CastSocketService overrides.
  std::unique_ptr<CastSocket> RemoveSocket(int channel_id) override;
  CastSocket* GetSocket(int channel_id) const override;
  CastSocket* GetSocket(const net::IPEndPoint& ip_endpoint) const override;
  void OpenSocket(NetworkContextGetter network_context_getter,
                  const CastSocketOpenParams& open_params,
                  CastSocket::OnOpenCallback open_cb) override;
  void AddObserver(CastSocket::Observer* observer) override;
  void RemoveObserver(CastSocket::Observer* observer) override;

  // openscreen::cast::CastSocket::Client overrides.
  void OnError(LibcastSocket* socket, openscreen::Error error) override;
  void OnMessage(LibcastSocket* socket,
                 ::cast::channel::CastMessage message) override;

  // openscreen::cast::SenderSocketFactory::Client overrides.
  void OnConnected(openscreen::cast::SenderSocketFactory* factory,
                   const openscreen::IPEndpoint& endpoint,
                   std::unique_ptr<LibcastSocket> socket) override;
  void OnError(openscreen::cast::SenderSocketFactory* factory,
               const openscreen::IPEndpoint& endpoint,
               openscreen::Error error) override;

  void SetLibcastSocketForTest(std::unique_ptr<LibcastSocket> socket_for_test) {
    libcast_socket_for_test_ = std::move(socket_for_test);
  }

 private:
  friend class CastSocketService;
  friend class LibcastSocketServiceTest;

  using Sockets = std::map<int, std::unique_ptr<CastSocketWrapper>>;

  struct ConnectTimer {
    ConnectTimer(std::unique_ptr<base::CancelableOnceClosure> callback,
                 std::unique_ptr<base::OneShotTimer> timer);
    ConnectTimer(ConnectTimer&&);
    ~ConnectTimer();

    ConnectTimer& operator=(ConnectTimer&&);

    std::unique_ptr<base::CancelableOnceClosure> callback;
    std::unique_ptr<base::OneShotTimer> timer;
  };

  struct SavedOpenParams {
    base::TimeDelta ping_interval;
    base::TimeDelta liveness_timeout;
  };

  LibcastSocketService();

  bool EndpointPending(const net::IPEndPoint& ip_endpoint) const;

  void OnErrorSocketIOThread(LibcastSocket* socket, openscreen::Error error);
  void OnMessageIOThread(LibcastSocket* socket,
                         ::cast::channel::CastMessage message);

  void OnConnectedIOThread(openscreen::cast::SenderSocketFactory* factory,
                           const openscreen::IPEndpoint& endpoint,
                           std::unique_ptr<LibcastSocket> socket);
  void OnErrorIOThread(openscreen::cast::SenderSocketFactory* factory,
                       const openscreen::IPEndpoint& endpoint,
                       openscreen::Error error);

  void OnErrorBounce(LibcastSocket* socket, ChannelError error);

  // Used to generate CastSocket IDs on error, since the socket factory doesn't
  // provide us one in that case.
  static int last_channel_id_;

  // List of socket observers.
  base::ObserverList<CastSocket::Observer>::Unchecked observers_;

  openscreen_platform::TaskRunner openscreen_task_runner_;
  openscreen::cast::SenderSocketFactory socket_factory_;
  std::unique_ptr<openscreen::TlsConnectionFactory> tls_factory_;

  Sockets sockets_;
  std::map<openscreen::IPEndpoint, int> socket_endpoints_;

  // Data for pending connections.
  std::map<openscreen::IPEndpoint, ConnectTimer> pending_endpoints_;
  std::map<openscreen::IPEndpoint, std::vector<CastSocket::OnOpenCallback>>
      open_callbacks_;
  std::map<openscreen::IPEndpoint, SavedOpenParams> open_params_;

  std::unique_ptr<LibcastSocket> libcast_socket_for_test_;
};

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_LIBCAST_SOCKET_SERVICE_H_
