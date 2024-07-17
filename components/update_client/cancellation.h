// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CANCELLATION_H_
#define COMPONENTS_UPDATE_CLIENT_CANCELLATION_H_

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"

namespace update_client {

// Some update_client operations may be cancelled by the user while underway.
// When this happens, update_client conducts an orderly tear-down by
// interrupting ongoing work, and returning via the normal control flow (but
// most likely with error results instead of useful results).
//
// Depending on how much progress the operation has made, different steps may
// be required to interrupt and cancel ongoing work. A `Cancellation` is a
// container for these steps. When a function starts interruptible work, it
// should add a closure to interrupt the work to a `Cancellation` using
// `OnCancel`. When the work is completed or can no longer be interrupted, it
// should `Clear` the `Cancellation`. All functions of `Cancellation` must be
// called on the sequence on which the `Cancellation` was created.
class Cancellation : public base::RefCountedThreadSafe<Cancellation> {
 public:
  Cancellation();
  Cancellation(const Cancellation&) = delete;
  Cancellation& operator=(const Cancellation&) = delete;

  // Trigger cancellation: sets the state to cancelled, and calls any closure
  // registered via `OnCancel`.
  void Cancel();

  // Returns whether `Cancel` was previously called.
  [[nodiscard]] bool IsCancelled();

  // Registers a closure to be called when Cancel is called. If `Cancel` has
  // already been called, a task will be posted to run the callback. Only one
  // task may be registered at once.
  void OnCancel(base::OnceClosure callback);

  // Unregisters the cancellation callback.
  void Clear();

 private:
  friend class base::RefCountedThreadSafe<Cancellation>;
  ~Cancellation();

  SEQUENCE_CHECKER(sequence_checker_);
  bool cancelled_ = false;
  base::OnceClosure task_ = base::DoNothing();
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CANCELLATION_H_
