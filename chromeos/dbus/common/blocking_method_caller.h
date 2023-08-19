// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_COMMON_BLOCKING_METHOD_CALLER_H_
#define CHROMEOS_DBUS_COMMON_BLOCKING_METHOD_CALLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "dbus/message.h"

namespace dbus {

class Bus;
class Error;
class ObjectProxy;

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
  base::expected<std::unique_ptr<dbus::Response>, dbus::Error>
  CallMethodAndBlock(dbus::MethodCall* method_call);

 private:
  raw_ptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> proxy_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_COMMON_BLOCKING_METHOD_CALLER_H_
