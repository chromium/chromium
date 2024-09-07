// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_STUB_CELLULAR_NETWORKS_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_STUB_CELLULAR_NETWORKS_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

// Provides stub cellular for use by NetworkStateHandler. In this context,
// cellular stub networks correspond to networks shown exposed by
// NetworkStateHandler which are not backed by Shill.
//
// StubCellularNetworksProvider provides stub networks by utilizing
// CellularESimProfileHandler to obtain information about installed eSIM
// profiles and merge this information with networks known to Shill.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) StubCellularNetworksProvider
    : public NetworkStateHandler::StubCellularNetworksProvider {
 public:
  StubCellularNetworksProvider();
  ~StubCellularNetworksProvider() override;

  void Init(NetworkStateHandler* network_state_handler,
            CellularESimProfileHandler* cellular_esim_profile_handler,
            ManagedCellularPrefHandler* managed_cellular_pref_handler);

 private:
  friend class StubCellularNetworksProviderTest;

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

  // Returns an IccidEidPair corresponding to each eSIM profile and each SIM
  // slot.
  std::vector<IccidEidPair> GetESimAndSlotMetadata(const DeviceState* device);

  // Adds stub cellular networks to |new_stub_networks| list for the metadata in
  // |esim_and_slot_metadata| that does not correspond to networks already
  // present in not already in |all_iccids|.
  bool AddStubNetworks(
      const DeviceState* device,
      const std::vector<IccidEidPair>& esim_and_slot_metadata,
      const base::flat_set<std::string>& all_iccids,
      NetworkStateHandler::ManagedStateList& new_stub_networks);

  // Removes all non-Shill stub cellular networks from |network_list| that are
  // not required anymore viz. 1) Stub networks for which a corresponding entry
  // already exists in |shill_iccids| 2) Stub networks that do not have
  // corresponding entry in |esim_and_slot_metadata| or a slot info entry on
  // given |device| (e.g. eSIM profile was removed or pSIM was removed from the
  // slot) 3) Stub networks that do not reflect the correct managed by policy
  // state. If both |esim_slot_metadata| and |shill_iccids| are nullptr, then
  // all stub networks are removed.
  bool RemoveCellularNetworks(
      const std::vector<IccidEidPair>* esim_and_slot_metadata,
      const base::flat_set<std::string>* shill_iccids,
      NetworkStateHandler::ManagedStateList& network_list);

  // Iterates through the provided stub networks and updates any network that is
  // found to have an outdated managed state. This allows the UI to more quickly
  // update when a cellular network transitions to become unmanaged or
  // vice-versa.
  bool UpdateCellularNetworks(
      NetworkStateHandler::ManagedStateList& network_list);

  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<CellularESimProfileHandler> cellular_esim_profile_handler_ = nullptr;
  raw_ptr<ManagedCellularPrefHandler, DanglingUntriaged>
      managed_cellular_pref_handler_ = nullptr;

  // Map which stores the GUID used for stubs created by this class. Each
  // network should use a consistent GUID throughout a session.
  base::flat_map<std::string, std::string> iccid_to_guid_map_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_STUB_CELLULAR_NETWORKS_PROVIDER_H_
