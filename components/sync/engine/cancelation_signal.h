// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CANCELATION_SIGNAL_H_
#define COMPONENTS_SYNC_ENGINE_CANCELATION_SIGNAL_H_

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"

namespace syncer {

// This class is used to allow one thread to request that another abort and
// return early.
//
// The signalling thread owns this class and my call Signal() at any time.
// After that call, this class' IsSignalled() will always return true.  The
// intended use case is that the task intending to support early exit will
// periodically check the value of IsSignalled() to see if it should return
// early.
//
// The receiving task may also choose to register an observer whose
// OnCancelationSignalReceived() method will be executed on the signaller's
// thread when Signal() is called.  This may be used for sending an early
// Signal() to a WaitableEvent.  The registration of the handler is necessarily
// racy.  If Signal() is executes before TryRegisterHandler(),
// TryRegisterHandler() will not perform any registration and return false. That
// function's caller must handle this case.
//
// This class supports only one handler, though it could easily support multiple
// observers if we found a use case for such a feature.
class CancelationSignal {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    // This may be called from a foreign thread while the CancelationSignal's
    // lock is held.  The callee should avoid performing slow or blocking
    // operations.
    virtual void OnCancelationSignalReceived() = 0;
  };

  CancelationSignal();
  ~CancelationSignal();

  // Tries to register a handler to be invoked when Signal() is called.
  //
  // If Signal() has already been called, returns false without registering
  // the handler.  Returns true when the registration is successful.
  //
  // If the registration was successful, the handler must be unregistered with
  // UnregisterHandler before this CancelationSignal is destroyed.
  bool TryRegisterHandler(Observer* handler);

  // Unregisters the abort handler.
  void UnregisterHandler(Observer* handler);

  // Returns true if Signal() has been called.
  bool IsSignalled();

  // Sets the stop_requested_ flag and calls the OnCancelationSignalReceived()
  // method of the registered handler, if there is one registered at the time.
  // SignalReceived() will be called with the |signal_lock_| held.
  void Signal();

 private:
  // Protects all members of this class.
  base::Lock signal_lock_;

  // True if Signal() has been invoked.
  bool signalled_ = false;

  // The registered abort handler.  May be null.
  raw_ptr<Observer> handler_ = nullptr;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CANCELATION_SIGNAL_H_
