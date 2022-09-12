// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_COMMON_BLOCKING_METHOD_CALLER_H_
#define CHROMEOS_DBUS_COMMON_BLOCKING_METHOD_CALLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "dbus/message.h"

namespace dbus {

class Bus;
class ObjectProxy;
class ScopedDBusError;

}  // namespace dbus

namespace chromeos {

// A utility class to call D-Bus methods in a synchronous (blocking) way.
// Note: Blocking the thread until it returns is not a good idea in most cases.
//       Avoid using this class as hard as you can.
class COMPONENT_EXPORT(CHROMEOS_DBUS_COMMON) BlockingMethodCaller {
 public:
  BlockingMethodCaller(dbus::Bus* bus, dbus::ObjectProxy* proxy);

  BlockingMethodCaller(const BlockingMethodCaller&) = delete;
  BlockingMethodCaller& operator=(const BlockingMethodCaller&) = delete;

  virtual ~BlockingMethodCaller();

  // Calls the method and blocks until it returns.
  std::unique_ptr<dbus::Response> CallMethodAndBlock(
      dbus::MethodCall* method_call);

  // Calls the method and blocks until it returns. Populates the |error| and
  // returns null in case of an error.
  std::unique_ptr<dbus::Response> CallMethodAndBlockWithError(
      dbus::MethodCall* method_call,
      dbus::ScopedDBusError* error_out);

 private:
  raw_ptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> proxy_;
  base::WaitableEvent on_blocking_method_call_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_COMMON_BLOCKING_METHOD_CALLER_H_
