// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_CANCELATION_OBSERVER_H_
#define COMPONENTS_SYNC_BASE_CANCELATION_OBSERVER_H_

namespace syncer {

// Interface for classes that handle signals from the CancelationSignal.
class CancelationObserver {
 public:
  CancelationObserver() = default;
  virtual ~CancelationObserver() = default;

  // This may be called from a foreign thread while the CancelationSignal's lock
  // is held.  The callee should avoid performing slow or blocking operations.
  virtual void OnSignalReceived() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_CANCELATION_OBSERVER_H_
