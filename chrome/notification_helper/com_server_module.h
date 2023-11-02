// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NOTIFICATION_HELPER_COM_SERVER_MODULE_H_
#define CHROME_NOTIFICATION_HELPER_COM_SERVER_MODULE_H_

#include "base/synchronization/waitable_event.h"
#include "base/win/windows_types.h"

namespace notification_helper {

// This class is used to host the NotificationActivator COM component and serve
// as the module for an out-of-proc COM server.
class ComServerModule {
 public:
  ComServerModule();

  ComServerModule(const ComServerModule&) = delete;
  ComServerModule& operator=(const ComServerModule&) = delete;

  ~ComServerModule();

  // Handles object registration and unregistration. Returns when all registered
  // objects are released.
  HRESULT Run();

  // Registers the NotificationActivator COM object so other applications can
  // connect to it. Returns the registration status.
  HRESULT RegisterClassObjects();

  // Unregisters the NotificationActivator COM object. Returns the
  // unregistration status.
  HRESULT UnregisterClassObjects();

  // Returns the state of the event.
  bool IsEventSignaled();

 private:
  // Waits for all instance objects are released from the module.
  void WaitForZeroObjectCount();

  // Sends out the signal that all objects are now released from the module.
  void SignalObjectCountZero();

  // Identifiers of registered class objects. Used for unregistration.
  DWORD cookies_[1] = {0};

  // This event starts "unsignaled", and is signaled when the last instance
  // object is released from the module.
  base::WaitableEvent object_zero_count_;
};

}  // namespace notification_helper

#endif  // CHROME_NOTIFICATION_HELPER_COM_SERVER_MODULE_H_
