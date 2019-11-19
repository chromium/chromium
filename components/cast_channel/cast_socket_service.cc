// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_socket_service.h"

#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/logger.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace cast_channel {

int CastSocketService::last_channel_id_ = 0;

CastSocketService::CastSocketService()
    : logger_(new Logger()),
      // IO thread's task runner is used because of:
      // (1) ChromeURLRequestContextGetter::GetURLRequestContext, which is
      // called by CastMediaSinkServiceImpl, must run on IO thread. (2) Parts of
      // CastChannel extension API functions run on IO thread.
      task_runner_(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::IO})) {}

// This is a leaky singleton and the dtor won't be called.
CastSocketService::~CastSocketService() = default;

// static
CastSocketService* CastSocketService::GetInstance() {
  static CastSocketService* instance = new CastSocketService();
  return instance;
}

scoped_refptr<Logger> CastSocketService::GetLogger() {
  return logger_;
}

CastSocket* CastSocketService::AddSocket(std::unique_ptr<CastSocket> socket) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(socket);
  int id = ++last_channel_id_;
  socket->set_id(id);

  auto* socket_ptr = socket.get();
  sockets_.insert(std::make_pair(id, std::move(socket)));
  return socket_ptr;
}

std::unique_ptr<CastSocket> CastSocketService::RemoveSocket(int channel_id) {
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

CastSocket* CastSocketService::GetSocket(int channel_id) const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(channel_id > 0);
  const auto& socket_it = sockets_.find(channel_id);
  return socket_it == sockets_.end() ? nullptr : socket_it->second.get();
}

CastSocket* CastSocketService::GetSocket(
    const net::IPEndPoint& ip_endpoint) const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto it = std::find_if(
      sockets_.begin(), sockets_.end(),
      [&ip_endpoint](
          const std::pair<const int, std::unique_ptr<CastSocket>>& pair) {
        return pair.second->ip_endpoint() == ip_endpoint;
      });
  return it == sockets_.end() ? nullptr : it->second.get();
}

void CastSocketService::OpenSocket(NetworkContextGetter network_context_getter,
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

void CastSocketService::AddObserver(CastSocket::Observer* observer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(observer);
  if (observers_.HasObserver(observer))
    return;

  observers_.AddObserver(observer);
  for (auto& socket_it : sockets_)
    socket_it.second->AddObserver(observer);
}

void CastSocketService::RemoveObserver(CastSocket::Observer* observer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(observer);

  for (auto& socket_it : sockets_)
    socket_it.second->RemoveObserver(observer);
  observers_.RemoveObserver(observer);
}

void CastSocketService::SetSocketForTest(
    std::unique_ptr<cast_channel::CastSocket> socket_for_test) {
  socket_for_test_ = std::move(socket_for_test);
}

}  // namespace cast_channel
