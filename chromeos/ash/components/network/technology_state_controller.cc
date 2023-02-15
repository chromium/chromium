// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/technology_state_controller.h"

#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

TechnologyStateController::TechnologyStateController() = default;

TechnologyStateController::~TechnologyStateController() = default;

void TechnologyStateController::Init(
    NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
}

void TechnologyStateController::SetTechnologiesEnabled(
    const NetworkTypePattern& type,
    bool enabled,
    network_handler::ErrorCallback error_callback) {
  network_state_handler_->SetTechnologyEnabled(type, enabled,
                                               std::move(error_callback));
}

}  // namespace ash