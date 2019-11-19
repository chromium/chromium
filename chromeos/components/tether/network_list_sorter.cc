// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_list_sorter.h"

#include <algorithm>

#include "base/logging.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"

namespace chromeos {

namespace tether {

namespace {

// Returns true if |first_state| should be ordered before |second_state| in the
// output list.
bool CompareStates(const std::unique_ptr<ManagedState>& first_state,
                   const std::unique_ptr<ManagedState>& second_state) {
  const NetworkState* first = first_state->AsNetworkState();
  const NetworkState* second = second_state->AsNetworkState();

  // Priority 1: Prefer connected networks to non-connected.
  if (first->IsConnectedState() != second->IsConnectedState())
    return first->IsConnectedState();

  // Priority 2: Prefer connecting networks to non-connecting.
  if (first->IsConnectingState() != second->IsConnectingState())
    return first->IsConnectingState();

  // Priority 3: Prefer higher signal strength.
  if (first->signal_strength() != second->signal_strength())
    return first->signal_strength() > second->signal_strength();

  // Priority 4: Prefer higher battery percentage.
  if (first->battery_percentage() != second->battery_percentage())
    return first->battery_percentage() > second->battery_percentage();

  // Priority 5: Prefer devices which have already connected before.
  if (first->tether_has_connected_to_host() !=
      second->tether_has_connected_to_host()) {
    return first->tether_has_connected_to_host();
  }

  // Priority 6: Alphabetize by device name.
  if (first->name() != second->name())
    return first->name() < second->name();

  // Priority 7: Alphabetize by cellular carrier name.
  if (first->tether_carrier() != second->tether_carrier())
    return first->tether_carrier() < second->tether_carrier();

  // Priority 8: Alphabetize by GUID. GUID is not shown in the UI,
  // so this is just a tie-breaker. All networks have unique GUIDs.
  DCHECK(first->guid() != second->guid());
  return first->guid() < second->guid();
}

}  // namespace

NetworkListSorter::NetworkListSorter() = default;

NetworkListSorter::~NetworkListSorter() = default;

void NetworkListSorter::SortTetherNetworkList(
    NetworkStateHandler::ManagedStateList* tether_networks) const {
  std::sort(tether_networks->begin(), tether_networks->end(), &CompareStates);
}

}  // namespace tether

}  // namespace chromeos
