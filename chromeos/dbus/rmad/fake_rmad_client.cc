// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/rmad/fake_rmad_client.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeRmadClient::FakeRmadClient() {
  abort_rma_reply_.set_error(rmad::RMAD_ERROR_OK);
}
FakeRmadClient::~FakeRmadClient() = default;

void FakeRmadClient::GetCurrentState(
    DBusMethodCallback<rmad::GetStateReply> callback) {
  if (NumStates() > 0) {
    CHECK(state_index_ < NumStates());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetStateReply()));
  } else {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
  }
}

void FakeRmadClient::TransitionNextState(
    const rmad::RmadState& state,
    DBusMethodCallback<rmad::GetStateReply> callback) {
  if (NumStates() == 0) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  CHECK_LT(state_index_, NumStates());
  if (state.state_case() != GetStateCase()) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_REQUEST_INVALID);
    reply.set_allocated_state(new rmad::RmadState(GetState()));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  if (state_index_ >= NumStates() - 1) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_TRANSITION_FAILED);
    reply.set_allocated_state(new rmad::RmadState(GetState()));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  // Update the fake state with the new data.
  if (state_index_ < NumStates()) {
    // TODO(gavindodd): Maybe the state should not update if the existing state
    // has an error?
    state_replies_[state_index_].set_allocated_state(
        new rmad::RmadState(state));
  }

  state_index_++;
  CHECK_LT(state_index_, NumStates());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetStateReply()));
}

void FakeRmadClient::TransitionPreviousState(
    DBusMethodCallback<rmad::GetStateReply> callback) {
  if (NumStates() == 0) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  CHECK_LT(state_index_, NumStates());
  if (state_index_ == 0) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_TRANSITION_FAILED);
    reply.set_allocated_state(new rmad::RmadState(GetState()));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  state_index_--;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetStateReply()));
}

void FakeRmadClient::AbortRma(
    DBusMethodCallback<rmad::AbortRmaReply> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     absl::optional<rmad::AbortRmaReply>(abort_rma_reply_)));
}

void FakeRmadClient::SetFakeStateReplies(
    std::vector<rmad::GetStateReply> fake_states) {
  state_replies_ = std::move(fake_states);
  state_index_ = 0;
}

void FakeRmadClient::SetAbortable(bool abortable) {
  abort_rma_reply_.set_error(abortable ? rmad::RMAD_ERROR_OK
                                       : rmad::RMAD_ERROR_CANNOT_CANCEL_RMA);
}

const rmad::GetStateReply& FakeRmadClient::GetStateReply() const {
  return state_replies_[state_index_];
}

const rmad::RmadState& FakeRmadClient::GetState() const {
  return GetStateReply().state();
}

rmad::RmadState::StateCase FakeRmadClient::GetStateCase() const {
  return GetState().state_case();
}

size_t FakeRmadClient::NumStates() const {
  return state_replies_.size();
}

}  // namespace chromeos
