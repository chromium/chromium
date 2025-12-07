// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cancelation_signal.h"

#include "base/check_op.h"

namespace syncer {

CancelationSignal::CancelationSignal() = default;

CancelationSignal::~CancelationSignal() {
  DCHECK(!handler_);
}

bool CancelationSignal::TryRegisterHandler(Observer* handler) {
  base::AutoLock lock(signal_lock_);
  DCHECK(!handler_);

  if (signalled_) {
    return false;
  }

  handler_ = handler;
  return true;
}

void CancelationSignal::UnregisterHandler(Observer* handler) {
  base::AutoLock lock(signal_lock_);
  DCHECK_EQ(handler_, handler);
  handler_ = nullptr;
}

bool CancelationSignal::IsSignalled() {
  base::AutoLock lock(signal_lock_);
  return signalled_;
}

void CancelationSignal::Signal() {
  base::AutoLock lock(signal_lock_);
  DCHECK(!signalled_);

  signalled_ = true;
  if (handler_) {
    handler_->OnCancelationSignalReceived();
  }
}

}  // namespace syncer
