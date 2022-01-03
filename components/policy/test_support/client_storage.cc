// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/client_storage.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "crypto/sha2.h"

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

const ClientStorage::ClientInfo* ClientStorage::LookupByStateKey(
    const std::string& state_key) const {
  for (auto const& [device_id, client_info] : clients_) {
    if (base::Contains(client_info.state_keys, state_key))
      return &client_info;
  }
  return nullptr;
}

size_t ClientStorage::GetNumberOfRegisteredClients() const {
  return clients_.size();
}

std::vector<std::string> ClientStorage::GetMatchingStateKeyHashes(
    uint64_t modulus,
    uint64_t remainder) const {
  std::vector<std::string> hashes;
  for (const auto& [device_id, client_info] : clients_) {
    // This does not actually divide hashes by |modulus| and verify that
    // |remainder| is correct as current tests do not rely on this behavior.
    // This is difficult to implement since 32-byte hashes do not fit into
    // regular integer types and thus long-arithmetic approach is needed.
    for (const std::string& key : client_info.state_keys)
      hashes.push_back(crypto::SHA256HashString(key));
  }
  return hashes;
}

}  // namespace policy
