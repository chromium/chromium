// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_CLIENT_STORAGE_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_CLIENT_STORAGE_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace policy {

// Stores information about registered clients.
class ClientStorage {
 public:
  struct ClientInfo {
    ClientInfo();
    ClientInfo(const ClientInfo& client_info);
    ClientInfo& operator=(const ClientInfo& client_info);
    ClientInfo(ClientInfo&& client_info);
    ClientInfo& operator=(ClientInfo&& client_info);
    ~ClientInfo();

    std::string device_id;
    std::string device_token;
    std::string machine_name;
    std::optional<std::string> username;
    std::vector<std::string> state_keys;
    std::set<std::string> allowed_policy_types;
  };

  ClientStorage();
  ClientStorage(ClientStorage&& client_storage);
  ClientStorage& operator=(ClientStorage&& client_storage);
  ~ClientStorage();

  // Register client so the server returns policy without the client having to
  // make a registration call. This could be called at any time (before or after
  // starting the server).
  void RegisterClient(const ClientInfo& client_info);

  // Returns true if there is a client associated with |device_id|.
  bool HasClient(const std::string& device_id) const;

  // Returns the client info associated with |device_id|. Fails if there is no
  // such a client.
  const ClientInfo& GetClient(const std::string& device_id) const;

  // Returns the client info associated with |device_id| or nullptr if there is
  // no such a client.
  const ClientInfo* GetClientOrNull(const std::string& device_id) const;

  // Returns the client info associated with |state_key| or nullptr if there is
  // no such a client.
  const ClientInfo* LookupByStateKey(const std::string& state_key) const;

  // Returns true if deletion of client with token |device_token| succeeded.
  bool DeleteClient(const std::string& device_token);

  // Returns the number of clients registered.
  size_t GetNumberOfRegisteredClients() const;

  // Returns hashes for all state keys registered with the server, which, when
  // divied by |modulus|, result in the specified |remainder|.
  std::vector<std::string> GetMatchingStateKeyHashes(uint64_t modulus,
                                                     uint64_t remainder) const;

  // Returns all the clients in the storage.
  std::vector<ClientInfo> GetAllClients();

 private:
  // Key: device ids.
  std::map<std::string, ClientInfo> clients_;
  // Maps device tokens to device IDs.
  std::map<std::string, std::string> registered_tokens_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_CLIENT_STORAGE_H_
