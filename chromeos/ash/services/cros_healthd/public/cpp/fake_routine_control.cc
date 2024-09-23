// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/public/cpp/fake_routine_control.h"

#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cros_healthd {

FakeRoutineControl::FakeRoutineControl(
    mojo::PendingReceiver<mojom::RoutineControl> pending_receiver,
    mojo::PendingRemote<mojom::RoutineObserver> observer)
    : receiver_(this, std::move(pending_receiver)) {
  if (observer.is_valid()) {
    routine_observer_.Bind(std::move(observer));

    auto init_state = mojom::RoutineState::New();
    init_state->percentage = 0;
    init_state->state_union = mojom::RoutineStateUnion::NewInitialized(
        mojom::RoutineStateInitialized::New());
    routine_observer_->OnRoutineStateChange(std::move(init_state));
  }
}

FakeRoutineControl::~FakeRoutineControl() = default;

void FakeRoutineControl::GetState(GetStateCallback callback) {
  std::move(callback).Run(get_state_response_->Clone());
}

void FakeRoutineControl::Start() {
  start_called_ = true;
}

void FakeRoutineControl::ReplyInquiry(mojom::RoutineInquiryReplyPtr reply) {
  last_inquiry_reply_ = std::move(reply);
}

void FakeRoutineControl::SetGetStateResponse(mojom::RoutineStatePtr& state) {
  get_state_response_.Swap(&state);
}

mojom::RoutineInquiryReplyPtr FakeRoutineControl::GetLastInquiryReply() {
  return last_inquiry_reply_.Clone();
}

mojo::Remote<mojom::RoutineObserver>* FakeRoutineControl::GetObserver() {
  if (routine_observer_.is_bound()) {
    return &routine_observer_;
  }
  return nullptr;
}

mojo::Receiver<mojom::RoutineControl>* FakeRoutineControl::GetReceiver() {
  return &receiver_;
}

}  // namespace ash::cros_healthd
