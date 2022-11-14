// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/wait_for_signal_or_timeout.h"
#include "base/task/sequenced_task_runner.h"

WaitForSignalOrTimeout::WaitForSignalOrTimeout() = default;
WaitForSignalOrTimeout::~WaitForSignalOrTimeout() = default;

void WaitForSignalOrTimeout::Signal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  SignalHandler(/*triggered_by_signal=*/true);
}

bool WaitForSignalOrTimeout::IsSignaled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  return state_ == State::kSignalReceived || state_ == State::kDone;
}

void WaitForSignalOrTimeout::OnEventOrTimeOut(Callback callback,
                                              base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  switch (state_) {
    case State::kInitialState:
      callback_ = std::move(callback);
      ++generation_id_;  // Invalidate previous OnTimeOut tasks.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&WaitForSignalOrTimeout::OnTimeOut,
                         weak_factory_.GetWeakPtr(), generation_id_),
          timeout);
      break;

    case State::kSignalReceived:
      state_ = State::kDone;
      std::move(callback).Run(
          /*triggered_by_signal=*/in_state_signal_received_due_to_signal_call_);
      break;

    case State::kDone:
      Reset();
      OnEventOrTimeOut(std::move(callback), timeout);
      break;
  }
}

void WaitForSignalOrTimeout::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  state_ = State::kInitialState;
  ++generation_id_;
  callback_ = Callback();
}

void WaitForSignalOrTimeout::OnTimeOut(int generation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  if (generation_id == generation_id_)
    SignalHandler(/*triggered_by_signal=*/false);
}

void WaitForSignalOrTimeout::SignalHandler(bool triggered_by_signal) {
  switch (state_) {
    case State::kInitialState:
      if (callback_.is_null()) {
        state_ = State::kSignalReceived;
        in_state_signal_received_due_to_signal_call_ = triggered_by_signal;
      } else {
        state_ = State::kDone;
        std::move(callback_).Run(triggered_by_signal);
      }
      break;

    case State::kSignalReceived:
    case State::kDone:
      break;
  }
}
