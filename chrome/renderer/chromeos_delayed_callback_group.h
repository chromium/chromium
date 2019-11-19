// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROMEOS_DELAYED_CALLBACK_GROUP_H_
#define CHROME_RENDERER_CHROMEOS_DELAYED_CALLBACK_GROUP_H_

#include <functional>
#include <list>
#include <queue>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

// Manage a collection of callbacks to be run en mass when RunAll() is called.
// This class is thread-safe.
class DelayedCallbackGroup
    : public base::RefCountedThreadSafe<DelayedCallbackGroup> {
 public:
  // The reason for the callback being invokes.
  enum class RunReason {
    // The callback is being run normally i.e. RunAll() was called.
    NORMAL,
    // The timeout period elapsed before RunAll() was invoked.
    TIMEOUT
  };

  using Callback = base::OnceCallback<void(RunReason)>;

  // All callbacks will be run when RunAll() is called or after the expiration
  // delay specified by |expiration_delay|.
  DelayedCallbackGroup(
      base::TimeDelta expiration_delay,
      scoped_refptr<base::SequencedTaskRunner> expiration_task_runner);

  // Add a |callback| to the queue to be called at a later time on the calling
  // sequence task runner. |callback| will either be called when RunAll() is
  // called or if a delay of |expiration_delay| has elapsed after calling
  // Add() without RunAll() being called first.
  //
  // Callbacks are called in the same order they were added.
  void Add(Callback callback);

  // Run all non-expired callback managed by this instance in the order in which
  // they were added via Add(). All callbacks will be passed the
  // RunReason::NORMAL parameter value.
  void RunAll();

 private:
  friend class base::RefCountedThreadSafe<DelayedCallbackGroup>;

  struct CallbackEntry {
    CallbackEntry(
        Callback callback,
        const scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
        base::TimeTicks expiration_time);
    ~CallbackEntry();

    Callback callback_;
    const scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
    base::TimeTicks expiration_time_;
  };

  ~DelayedCallbackGroup();

  void StartExpirationTimer();
  void CancelExpirationTimer();
  void OnExpirationTimer();

  // Call all expired callbacks with the RunReason::TIMEOUT parameter value.
  void ProcessExpiredCallbacks(base::TimeTicks expiration_time);

  // Call all remaining callbacks with the RunReason::TIMEOUT parameter value.
  void ExpireAllCallbacks() EXCLUSIVE_LOCKS_REQUIRED(callbacks_lock_);

  std::queue<CallbackEntry, std::list<CallbackEntry>> callbacks_
      GUARDED_BY(callbacks_lock_);
  base::TimeDelta expiration_delay_ GUARDED_BY(callbacks_lock_);
  base::CancelableOnceClosure expiration_timeout_;
  mutable base::Lock callbacks_lock_;

  scoped_refptr<base::SequencedTaskRunner> expiration_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DelayedCallbackGroup);
};

#endif  // CHROME_RENDERER_CHROMEOS_DELAYED_CALLBACK_GROUP_H_
