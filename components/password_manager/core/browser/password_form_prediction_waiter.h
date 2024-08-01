// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_PREDICTION_WAITER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_PREDICTION_WAITER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class OneShotTimer;
}

namespace password_manager {

// Filling timeout for waiting for asynchronous predictions.
inline constexpr base::TimeDelta kMaxFillingDelayForAsyncPredictions =
    base::Milliseconds(500);

// Helper class for PasswordFormManager to manage outstanding asynchronous
// prediction fetches. This issues callbacks to wait on multiple
// asynchronous events, signalling when all are complete, and also a timer
// that will signal OnTimeout() if there are still outstanding callbacks.
// It is possible for both OnTimeout() and OnWaitCompleted() to be called if
// OnTimeout() is called first. If the timer is not active, every closure
// invocation will cause a call to OnWaitCompleted().
class PasswordFormPredictionWaiter {
 public:
  class Client {
   public:
    virtual void OnWaitCompleted() = 0;
    virtual void OnTimeout() = 0;
  };

  // This is used for metrics and must be kept in sync with the corresponding
  // entry in tools/metrics/histograms/metadata/password/enums.xml.
  // Entries should not be renumbered or reused.
  enum class WaitResult {
    kNoTimeout = 0,
    kTimeoutWaitingForOneClosure = 1,
    kTimeoutWaitingForTwoOrMoreClosures = 2,

    kMaxValue = kTimeoutWaitingForTwoOrMoreClosures,
  };

  explicit PasswordFormPredictionWaiter(Client* client);

  PasswordFormPredictionWaiter(const PasswordFormPredictionWaiter&) = delete;
  PasswordFormPredictionWaiter& operator=(const PasswordFormPredictionWaiter&) =
      delete;

  ~PasswordFormPredictionWaiter();

  void StartTimer();

  // Resets the timer and `outstanding_closures_`.
  void Reset();

  // Returns whether the waiter is currently active and waiting.
  bool IsActive() const;

  // Issues a new closure that should be invoked when a task is completed.
  // When the timer is active, all issued closures have to be invoked before
  // the Client's OnWaitCompleted() method is called.
  // If the timer has expired or has not been set, then any single closure
  // invocation will result in a call to OnWaitCompleted().
  base::OnceClosure CreateClosure();

 private:
  void OnTimeout();
  void OnClosureComplete();

  // The client owns the waiter so this pointer will survive this object's
  // lifetime.
  raw_ptr<Client> client_;

  base::OneShotTimer timer_;

  // Tracks the number of outstanding closure. Notifies the client when
  // a closure is activated and this returns to 0.
  int outstanding_closures_ = 0;

  base::WeakPtrFactory<PasswordFormPredictionWaiter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_FORM_PREDICTION_WAITER_H_
