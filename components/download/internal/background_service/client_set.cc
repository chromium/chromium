// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/debugging_client.h"

namespace download {

ClientSet::ClientSet(std::unique_ptr<DownloadClientMap> clients)
    : clients_(std::move(clients)) {
  DCHECK(clients_->find(DownloadClient::INVALID) == clients_->end());
  DCHECK(clients_->find(DownloadClient::DEBUGGING) == clients_->end());

  // Add a Client to handle debug downloads automatically.
  clients_->insert(std::make_pair(DownloadClient::DEBUGGING,
                                  std::make_unique<DebuggingClient>()));
}

ClientSet::~ClientSet() = default;

std::set<DownloadClient> ClientSet::GetRegisteredClients() const {
  std::set<DownloadClient> clients;
  for (const auto& it : *clients_) {
    clients.insert(it.first);
  }

  return clients;
}

Client* ClientSet::GetClient(DownloadClient id) const {
  const auto& it = clients_->find(id);
  return it == clients_->end() ? nullptr : it->second.get();
}

}  // namespace download
