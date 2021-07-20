// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/client_storage.h"

#include "base/check.h"

namespace policy {

ClientStorage::ClientInfo::ClientInfo() = default;

ClientStorage::ClientInfo::ClientInfo(
    const ClientStorage::ClientInfo& client_info) = default;

ClientStorage::ClientInfo& ClientStorage::ClientInfo::operator=(
    const ClientStorage::ClientInfo& client_info) = default;

ClientStorage::ClientInfo::ClientInfo(ClientStorage::ClientInfo&& client_info) =
    default;

ClientStorage::ClientInfo& ClientStorage::ClientInfo::operator=(
    ClientStorage::ClientInfo&& client_info) = default;

ClientStorage::ClientInfo::~ClientInfo() = default;

ClientStorage::ClientStorage() = default;

ClientStorage::ClientStorage(ClientStorage&& client_storage) = default;

ClientStorage& ClientStorage::operator=(ClientStorage&& client_storage) =
    default;

ClientStorage::~ClientStorage() = default;

void ClientStorage::RegisterClient(const ClientInfo& client_info) {
  CHECK(!client_info.device_id.empty());

  clients_[client_info.device_id] = client_info;
}

bool ClientStorage::HasClient(const std::string& device_id) const {
  return clients_.find(device_id) != clients_.end();
}

const ClientStorage::ClientInfo& ClientStorage::GetClient(
    const std::string& device_id) const {
  const ClientInfo* const client_info = GetClientOrNull(device_id);
  CHECK(client_info);

  return *client_info;
}

const ClientStorage::ClientInfo* ClientStorage::GetClientOrNull(
    const std::string& device_id) const {
  auto it = clients_.find(device_id);
  return it == clients_.end() ? nullptr : &it->second;
}

size_t ClientStorage::GetNumberOfRegisteredClients() const {
  return clients_.size();
}

}  // namespace policy
