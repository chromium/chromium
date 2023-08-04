// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/cpp/fake_routine_controller.h"

#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cros_healthd {

FakeRoutineController::FakeRoutineController(
    mojo::PendingReceiver<mojom::RoutineControl> pending_receiver,
    mojo::PendingRemote<mojom::RoutineObserver> observer)
    : receiver_(this, std::move(pending_receiver)) {
  if (observer.is_valid()) {
    routine_observer_.Bind(std::move(observer));
  }
}

FakeRoutineController::~FakeRoutineController() = default;

void FakeRoutineController::GetState(GetStateCallback callback) {
  std::move(callback).Run(get_state_response_->Clone());
}

void FakeRoutineController::Start() {
  start_called_ = true;
}

void FakeRoutineController::SetGetStateResponse(mojom::RoutineStatePtr& state) {
  get_state_response_.Swap(&state);
}

mojo::Remote<mojom::RoutineObserver>* FakeRoutineController::GetObserver() {
  if (routine_observer_.is_bound()) {
    return &routine_observer_;
  }
  return nullptr;
}

mojo::Receiver<mojom::RoutineControl>* FakeRoutineController::GetReceiver() {
  return &receiver_;
}

}  // namespace ash::cros_healthd
