// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hidden_network_handler.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

HiddenNetworkHandler::HiddenNetworkHandler() {
  DCHECK(base::FeatureList::IsEnabled(features::kHiddenNetworkMigration));
}

void HiddenNetworkHandler::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConfigurationHandler* network_configuration_handler) {
  network_state_handler_ = network_state_handler;
  network_configuration_handler_ = network_configuration_handler;
}

}  // namespace ash
