// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_socket_service.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "components/media_router/common/providers/cast/channel/logger.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace cast_channel {

// static
CastSocketService* CastSocketService::GetInstance() {
  static CastSocketService* service = new CastSocketServiceImpl();
  return service;
}

CastSocketService::CastSocketService()
    : logger_(new Logger()),
      // IO thread's task runner is used because of:
      // (1) ChromeURLRequestContextGetter::GetURLRequestContext, which is
      // called by CastMediaSinkServiceImpl, must run on IO thread. (2) Parts of
      // CastChannel extension API functions run on IO thread.
      task_runner_(content::GetIOThreadTaskRunner({})) {}

CastSocketService::~CastSocketService() = default;

int CastSocketServiceImpl::last_channel_id_ = 0;

CastSocketServiceImpl::CastSocketServiceImpl() = default;

// This is a leaky singleton and the dtor won't be called.
CastSocketServiceImpl::~CastSocketServiceImpl() = default;

CastSocket* CastSocketServiceImpl::AddSocket(
    std::unique_ptr<CastSocket> socket) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(socket);
  CHECK(base::CheckAdd(last_channel_id_, 1).AssignIfValid(&last_channel_id_))
      << "Overflow in channel_id!";
  socket->set_id(last_channel_id_);

  auto* socket_ptr = socket.get();
  CHECK(sockets_.insert(std::make_pair(last_channel_id_, std::move(socket)))
            .second);
  return socket_ptr;
}

std::unique_ptr<CastSocket> CastSocketServiceImpl::RemoveSocket(
    int channel_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(channel_id > 0);
  auto socket_it = sockets_.find(channel_id);

  std::unique_ptr<CastSocket> socket;
  if (socket_it != sockets_.end()) {
    socket = std::move(socket_it->second);
    sockets_.erase(socket_it);
  }
  return socket;
}

void CastSocketService::CloseSocket(int channel_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto* socket = GetSocket(channel_id);
  if (socket) {
    socket->Close(base::BindOnce([](int x) {}));
    RemoveSocket(socket->id());
  }
}

CastSocket* CastSocketServiceImpl::GetSocket(int channel_id) const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(channel_id > 0);
  const auto& socket_it = sockets_.find(channel_id);
  return socket_it == sockets_.end() ? nullptr : socket_it->second.get();
}

CastSocket* CastSocketServiceImpl::GetSocket(
    const net::IPEndPoint& ip_endpoint) const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto it = base::ranges::find(sockets_, ip_endpoint,
                               [](const Sockets::value_type& pair) {
                                 return pair.second->ip_endpoint();
                               });
  return it == sockets_.end() ? nullptr : it->second.get();
}

void CastSocketServiceImpl::OpenSocket(
    network::NetworkContextGetter network_context_getter,
    const CastSocketOpenParams& open_params,
    CastSocket::OnOpenCallback open_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  const net::IPEndPoint& ip_endpoint = open_params.ip_endpoint;
  auto* socket = GetSocket(ip_endpoint);
  if (!socket) {
    // If cast socket does not exist.
    if (socket_for_test_) {
      socket = AddSocket(std::move(socket_for_test_));
    } else {
      socket = new CastSocketImpl(network_context_getter, open_params, logger_);
      AddSocket(base::WrapUnique(socket));
    }
  }

  for (auto& observer : observers_)
    socket->AddObserver(&observer);

  socket->Connect(std::move(open_cb));
}

void CastSocketServiceImpl::AddObserver(CastSocket::Observer* observer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(observer);
  if (observers_.HasObserver(observer))
    return;

  observers_.AddObserver(observer);
  for (auto& socket_it : sockets_)
    socket_it.second->AddObserver(observer);
}

void CastSocketServiceImpl::RemoveObserver(CastSocket::Observer* observer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(observer);

  for (auto& socket_it : sockets_)
    socket_it.second->RemoveObserver(observer);
  observers_.RemoveObserver(observer);
}

}  // namespace cast_channel
