// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_prediction_waiter.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"

namespace password_manager {

PasswordFormPredictionWaiter::PasswordFormPredictionWaiter(Client* client)
    : client_(client) {}

PasswordFormPredictionWaiter::~PasswordFormPredictionWaiter() = default;

void PasswordFormPredictionWaiter::StartTimer() {
  // Unretained |this| is safe because the timer will be destroyed on
  // destruction of this object.
  timer_.Start(FROM_HERE, kMaxFillingDelayForAsyncPredictions, this,
               &PasswordFormPredictionWaiter::OnTimeout);
}

void PasswordFormPredictionWaiter::InitializeClosure(size_t callback_count) {
  // Invalidating the weak pointers serves to cancel outstanding callbacks
  // on the BarrierClosure.
  weak_ptr_factory_.InvalidateWeakPtrs();
  barrier_closure_ = base::BarrierClosure(
      callback_count,
      base::BindOnce(&PasswordFormPredictionWaiter::OnClosureComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordFormPredictionWaiter::OnTimeout() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  barrier_closure_ = base::RepeatingClosure();
  client_->OnWaitCompleted();
}

void PasswordFormPredictionWaiter::OnClosureComplete() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  barrier_closure_ = base::RepeatingClosure();
  timer_.Stop();
  client_->OnWaitCompleted();
}

}  // namespace password_manager
