// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_COMMON_DBUS_METHOD_CALL_STATUS_H_
#define CHROMEOS_DBUS_COMMON_DBUS_METHOD_CALL_STATUS_H_

// TODO(hidehiko): Rename this file to dbus_callback.h, when we fully
// get rid of DBusMethodCallStatus enum defined below.

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {

class ObjectPath;

}  // namespace dbus

namespace chromeos {

// Callback to handle response of methods with result.
// If the method returns multiple values, std::tuple<...> will be used.
// In case of error, nullopt should be passed.
template <typename ResultType>
using DBusMethodCallback =
    base::OnceCallback<void(absl::optional<ResultType> result)>;

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

#endif  // CHROMEOS_DBUS_COMMON_DBUS_METHOD_CALL_STATUS_H_
