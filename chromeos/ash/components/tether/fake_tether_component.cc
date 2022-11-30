// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_tether_component.h"

namespace ash {

namespace tether {

FakeTetherComponent::FakeTetherComponent(bool has_asynchronous_shutdown)
    : has_asynchronous_shutdown_(has_asynchronous_shutdown) {}

FakeTetherComponent::~FakeTetherComponent() = default;

void FakeTetherComponent::FinishAsynchronousShutdown() {
  DCHECK(status() == TetherComponent::Status::SHUTTING_DOWN);
  TransitionToStatus(TetherComponent::Status::SHUT_DOWN);
}

void FakeTetherComponent::RequestShutdown(
    const ShutdownReason& shutdown_reason) {
  DCHECK(status() == TetherComponent::Status::ACTIVE);
  last_shutdown_reason_ = std::make_unique<ShutdownReason>(shutdown_reason);

  if (has_asynchronous_shutdown_)
    TransitionToStatus(TetherComponent::Status::SHUTTING_DOWN);
  else
    TransitionToStatus(TetherComponent::Status::SHUT_DOWN);
}

}  // namespace tether

}  // namespace ash
