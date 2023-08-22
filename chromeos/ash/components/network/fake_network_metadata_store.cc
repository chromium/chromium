// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/fake_network_metadata_store.h"

namespace ash {

FakeNetworkMetadataStore::FakeNetworkMetadataStore()
    : NetworkMetadataStore(/*network_configuration_handler=*/nullptr,
                           /*network_connection_handler=*/nullptr,
                           /*network_state_handler=*/nullptr,
                           /*managed_network_configuration_handler=*/nullptr,
                           /*profile_pref_service=*/nullptr,
                           /*device_pref_service=*/nullptr,
                           /*is_enterprise_managed=*/false) {}

FakeNetworkMetadataStore::~FakeNetworkMetadataStore() = default;

UserTextMessageSuppressionState
FakeNetworkMetadataStore::GetUserTextMessageSuppressionState(
    const std::string& guid) {
  if (auto it = text_message_suppression_state_map_.find(guid);
      it != text_message_suppression_state_map_.end()) {
    return it->second;
  }
  return UserTextMessageSuppressionState::kAllow;
}

void FakeNetworkMetadataStore::SetUserTextMessageSuppressionState(
    const std::string& guid,
    const UserTextMessageSuppressionState& state) {
  text_message_suppression_state_map_[guid] = state;
}

}  // namespace ash
