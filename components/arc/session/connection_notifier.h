// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_CONNECTION_NOTIFIER_H_
#define COMPONENTS_ARC_SESSION_CONNECTION_NOTIFIER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"

namespace arc {
namespace internal {

class ConnectionObserverBase;

// Manages events related to connection. Designed to be used only by
// ConnectionHolder.
class ConnectionNotifier {
 public:
  ConnectionNotifier();
  ~ConnectionNotifier();

  void AddObserver(ConnectionObserverBase* observer);
  void RemoveObserver(ConnectionObserverBase* observer);

  // Notifies observers that connection gets ready.
  void NotifyConnectionReady();

  // Notifies observers that connection is closed.
  void NotifyConnectionClosed();

 private:
  THREAD_CHECKER(thread_checker_);
  base::ObserverList<ConnectionObserverBase>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionNotifier);
};

}  // namespace internal
}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_CONNECTION_NOTIFIER_H_
