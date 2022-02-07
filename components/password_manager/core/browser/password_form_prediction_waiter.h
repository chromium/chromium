// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_PREDICTION_WAITER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_PREDICTION_WAITER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class OneShotTimer;
}

namespace password_manager {

// Filling timeout for waiting for asynchronous predictions.
constexpr base::TimeDelta kMaxFillingDelayForAsyncPredictions =
    base::Milliseconds(500);

// Helper class for PasswordFormManager to manage outstanding asynchronous
// prediction fetches. This uses a barrier callback to wait on multiple
// asynchronous events, signalling when all are complete, and also a timer
// that will cause cause Client::OnWaitCompleted() to be called even if there
// are still outstanding callbacks.
class PasswordFormPredictionWaiter {
 public:
  class Client {
   public:
    virtual void OnWaitCompleted() = 0;
  };

  explicit PasswordFormPredictionWaiter(Client* client);

  PasswordFormPredictionWaiter(const PasswordFormPredictionWaiter&) = delete;
  PasswordFormPredictionWaiter& operator=(const PasswordFormPredictionWaiter&) =
      delete;

  ~PasswordFormPredictionWaiter();

  void StartTimer();

  void InitializeClosure(size_t callback_count);
  const base::RepeatingClosure& closure() const { return barrier_closure_; }

 private:
  void OnTimeout();
  void OnClosureComplete();

  // The client owns the waiter so this pointer will survive this object's
  // lifetime.
  Client* client_;

  base::OneShotTimer timer_;

  // BarrierClosure is used to wait until predictions are obtained from
  // all asynchronous sources.
  base::RepeatingClosure barrier_closure_;

  base::WeakPtrFactory<PasswordFormPredictionWaiter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_PREDICTION_WAITER_H_
