// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_WAIT_FOR_SIGNAL_OR_TIMEOUT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_WAIT_FOR_SIGNAL_OR_TIMEOUT_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

// A WaitForSignalOrTimeout waits for Signal() or a time out and calls a
// callback when either of these happens for the first time.
//
// The WaitForSignalOrTimeout is Reset()-able and ensures that the callback will
// be called at most once (unless Reset() resets the state). The
// WaitForSignalOrTimeout can be destroyed at any time without negative
// side-effects. The callback won't be called in this case. If the Signal()
// arrives before a call for OnEventOrTimeOut(), the callback will be called
// immediately. If a second Signal() arrives, nothing happens. The
// WaitForSignalOrTimeout must be used on single task sequence.
//
// This class provides the bare minimum needed for a Payment task. If there are
// more use cases, feel free to spice it up and move it to base/.
class WaitForSignalOrTimeout {
 public:
  // The passed boolean is true if the callback happened by a call of Signal()
  // (as opposed to a timeout).
  using Callback = base::OnceCallback<void(bool)>;

  WaitForSignalOrTimeout();
  ~WaitForSignalOrTimeout();
  WaitForSignalOrTimeout(const WaitForSignalOrTimeout&) = delete;
  WaitForSignalOrTimeout& operator=(const WaitForSignalOrTimeout&) = delete;

  // Triggers |callback_| if it has not been called before, or registers that
  // the signal occurred, so that |callback| of OnEventOrTimeOut() can be
  // called immediately.
  void Signal();

  // Returns whether Signal() was called at least once or a timeout happened,
  // and Reset() has not been called afterwards. Note that this function does
  // not discriminate whether Signal() was called or a timeout happened.
  // The |callback_|'s parameter has this distinction, though.
  bool IsSignaled() const;

  // Registers the |callback| and calls it immediately if Signal() was called
  // already. Starts a timeout task, so that |callback| is called if no call of
  // Signal() is observed within |timeout|. A previous timeout is replaced by a
  // new one.
  void OnEventOrTimeOut(Callback callback, base::TimeDelta timeout);

  // Resets the state machine so that no Signal() was observed, no callback is
  // registered and no timeout task is running.
  void Reset();

 private:
  enum class State {
    // Signal() has not been called, yet.
    kInitialState,
    // Signal() has been called, but callback is not specified.
    kSignalReceived,
    // callback has been called.
    kDone,
  };

  // Internal callback for the timeout. |generation_id| is a generation counter
  // to ensure that old, delayed timeout tasks are ignored.
  void OnTimeOut(int generation_id);

  // Handler for Signal() and OnTimeOut(). Calls |callback_| if appropriate.
  // The parameter is true if this function is called via Signal() and false if
  // the function is called via OnTimeOut(). This parameter is passed to
  // callback.
  void SignalHandler(bool triggered_by_signal);

  State state_ = State::kInitialState;

  // This variable is only valid if state_ == State::kSignalReceived. It is
  // true if we moved into this state due to a Signal() call, and false if
  // we moved into this state due to an OnTimeOut() call.
  bool in_state_signal_received_due_to_signal_call_;

  // As the base::ThreadPool does not support cancelable tasks, we just rely on
  // a generation counter. Every time Reset() or OnEventOrTimeOut() are called,
  // the generation id is incremented. If outdated delayed OnTimeOut() tasks
  // trickle in, we recognize them as tasks for which the |generation_id|
  // parameter is less than the current generation_id_ and ignore them.
  int generation_id_ = 0;

  // Callback to be called in case of a Signal() or a time out.
  Callback callback_;

  SEQUENCE_CHECKER(my_sequence_checker_);

  base::WeakPtrFactory<WaitForSignalOrTimeout> weak_factory_{this};
};

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_WAIT_FOR_SIGNAL_OR_TIMEOUT_H_
