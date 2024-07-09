// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_COMMON_DBUS_CALLBACK_H_
#define CHROMEOS_DBUS_COMMON_DBUS_CALLBACK_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace dbus {

class ObjectPath;

}  // namespace dbus

namespace chromeos {

// Callback to handle response of methods with result.
// If the method returns multiple values, std::tuple<...> will be used.
// In case of error, nullopt should be passed.
template <typename ResultType>
using DBusMethodCallback =
    base::OnceCallback<void(std::optional<ResultType> result)>;

// Callback to handle response of methods without result.
// |result| is true if the method call is successfully completed, otherwise
// false.
using VoidDBusMethodCallback = base::OnceCallback<void(bool result)>;

// A callback to handle responses of methods returning a ObjectPath value that
// doesn't get call status.
using ObjectPathCallback =
    base::OnceCallback<void(const dbus::ObjectPath& result)>;

// Called when service becomes available.
using WaitForServiceToBeAvailableCallback =
    base::OnceCallback<void(bool service_is_available)>;

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_COMMON_DBUS_CALLBACK_H_
