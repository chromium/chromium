// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_STUB_CELLULAR_NETWORKS_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_STUB_CELLULAR_NETWORKS_PROVIDER_H_

#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

// Fake StubCellularNetworksProvider implementation which allows clients to
// specify ICCIDs of stub networks to be created.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) FakeStubCellularNetworksProvider
    : public NetworkStateHandler::StubCellularNetworksProvider {
 public:
  FakeStubCellularNetworksProvider();
  ~FakeStubCellularNetworksProvider() override;

  // Adds a stub network with the provided ICCID and EID. This network will be
  // used when a subsequent call to AddOrRemoveStubCellularNetworks() is made.
  // Note that an empty EID refers to a pSIM network.
  void AddStub(const std::string& stub_iccid,
               const std::string& eid = std::string(),
               bool is_managed = false);

  // Removes a stub network with the provided ICCID and EID, reversing a
  // previous call to AddStub().
  void RemoveStub(const std::string& stub_iccid,
                  const std::string& eid = std::string());

  size_t stub_networks_add_count() const { return stub_networks_add_count_; }

 private:
  using IccidEidPair = std::pair<std::string, std::string>;

  // NetworkStateHandler::StubCellularNetworksProvider:
  bool AddOrRemoveStubCellularNetworks(
      NetworkStateHandler::ManagedStateList& network_list,
      NetworkStateHandler::ManagedStateList& new_stub_networks,
      const DeviceState* device) override;
  bool GetStubNetworkMetadata(const std::string& iccid,
                              const DeviceState* cellular_device,
                              std::string* service_path_out,
                              std::string* guid_out) override;

  const std::string& GetGuidForStubIccid(const std::string& iccid);
  std::vector<IccidEidPair> GetStubsNotBackedByShill(
      const NetworkStateHandler::ManagedStateList& network_list) const;

  size_t stub_networks_add_count_ = 0;
  base::flat_set<IccidEidPair> stub_iccid_and_eid_pairs_;
  base::flat_map<std::string, std::string> iccid_to_guid_map_;
  base::flat_set<std::string> managed_iccids_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_STUB_CELLULAR_NETWORKS_PROVIDER_H_
