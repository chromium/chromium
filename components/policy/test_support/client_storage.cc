// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/client_storage.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
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
  registered_tokens_[client_info.device_token] = client_info.device_id;
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

bool ClientStorage::DeleteClient(const std::string& device_token) {
  auto it = registered_tokens_.find(device_token);
  if (it == registered_tokens_.end())
    return false;

  const std::string& device_id = it->second;
  DCHECK(!device_id.empty());
  auto it_clients = clients_.find(device_id);
  CHECK(it_clients != clients_.end(), base::NotFatalUntil::M130);

  clients_.erase(it_clients, clients_.end());
  registered_tokens_.erase(it, registered_tokens_.end());
  return true;
}

size_t ClientStorage::GetNumberOfRegisteredClients() const {
  return clients_.size();
}

std::vector<std::string> ClientStorage::GetMatchingStateKeyHashes(
    uint64_t modulus,
    uint64_t remainder) const {
  std::vector<std::string> hashes;
  for (const auto& [device_id, client_info] : clients_) {
    for (const std::string& key : client_info.state_keys) {
      std::string hash = crypto::SHA256HashString(key);
      uint64_t hash_remainder = 0;
      // Simulate long division in base 256, which allows us to interpret
      // individual chars in our hash as digits. We only care about the
      // remainder and hence do not compute the quotient in each iteration. This
      // assumes big-endian byte order.
      for (uint64_t digit : hash)
        hash_remainder = (hash_remainder * 256 + digit) % modulus;
      if (hash_remainder == remainder)
        hashes.push_back(hash);
    }
  }
  return hashes;
}

std::vector<ClientStorage::ClientInfo> ClientStorage::GetAllClients() {
  std::vector<ClientStorage::ClientInfo> result;
  for (const auto& [device_id, client_info] : clients_) {
    result.push_back(client_info);
  }
  return result;
}
}  // namespace policy
