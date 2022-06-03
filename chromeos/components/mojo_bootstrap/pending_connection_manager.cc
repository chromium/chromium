// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"

#include <utility>

#include "base/check_op.h"

namespace mojo_bootstrap {

// static
PendingConnectionManager& PendingConnectionManager::Get() {
  static base::NoDestructor<PendingConnectionManager> connection_manager;
  return *connection_manager;
}

bool PendingConnectionManager::OpenIpcChannel(const std::string& token,
                                              base::ScopedFD fd) {
  auto it = open_ipc_channel_callbacks_.find(token);
  if (it == open_ipc_channel_callbacks_.end()) {
    return false;
  }
  OpenIpcChannelCallback callback = std::move(it->second);
  open_ipc_channel_callbacks_.erase(it);
  std::move(callback).Run(std::move(fd));
  return true;
}

void PendingConnectionManager::ExpectOpenIpcChannel(
    base::UnguessableToken token,
    OpenIpcChannelCallback handler) {
  DCHECK(token);
  open_ipc_channel_callbacks_.emplace(token.ToString(), std::move(handler));
}

void PendingConnectionManager::CancelExpectedOpenIpcChannel(
    base::UnguessableToken token) {
  bool erased = open_ipc_channel_callbacks_.erase(token.ToString());
  DCHECK_EQ(1u, erased);
}

PendingConnectionManager::PendingConnectionManager() = default;
PendingConnectionManager::~PendingConnectionManager() = default;

}  // namespace mojo_bootstrap
