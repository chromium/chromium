// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_WAKE_LOCK_WAKE_LOCK_OBSERVER_H_
#define COMPONENTS_ARC_WAKE_LOCK_WAKE_LOCK_OBSERVER_H_

namespace arc {

// This is an interface for classes that want to learn when Android wake locks
// are acquired or released. Observer should register themselves by calling the
// overriding class's AddObserver() method.
class WakeLockObserver {
 public:
  virtual ~WakeLockObserver() = default;

  // Called when the tracked wake lock is acquired the first time i.e.
  // number of holders increases to 1.
  virtual void OnWakeLockAcquire() {}

  // Called when the tracked wake lock is released the last time i.e. the number
  // of holders goes to 0.
  virtual void OnWakeLockRelease() {}
};

}  // namespace arc

#endif  // COMPONENTS_ARC_WAKE_LOCK_WAKE_LOCK_OBSERVER_H_
