// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/mock_network_metadata_store.h"

namespace ash {

MockNetworkMetadataStore::MockNetworkMetadataStore()
    : NetworkMetadataStore(
          /*network_configuration_handler=*/nullptr,
          /*network_connection_handler=*/nullptr,
          /*network_state_handler=*/nullptr,
          /*managed_network_configuration_handler=*/nullptr,
          /*profile_pref_service=*/nullptr,
          /*device_pref_service=*/nullptr,
          /*is_enterprise_managed=*/false) {}

MockNetworkMetadataStore::~MockNetworkMetadataStore() = default;

}  // namespace ash
