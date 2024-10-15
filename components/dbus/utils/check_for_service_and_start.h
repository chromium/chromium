// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_CHECK_FOR_SERVICE_AND_START_H_
#define COMPONENTS_DBUS_UTILS_CHECK_FOR_SERVICE_AND_START_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}

namespace dbus_utils {

using CheckForServiceAndStartCallback =
    base::OnceCallback<void(/*service_started=*/std::optional<bool>)>;

// Checks whether the service `name` has an owner on the bus and runs `callback`
// with this boolean value. Attempts to autostart the service if possible.
// False is passed to the callback if the service is not activatable or failed to
// start. True is passed if the service is already running or was auto-started.
// Any DBus errors are indicated as nullopt.
COMPONENT_EXPORT(DBUS)
void CheckForServiceAndStart(scoped_refptr<dbus::Bus> bus,
                             const std::string& name,
                             CheckForServiceAndStartCallback callback);

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_CHECK_FOR_SERVICE_AND_START_H_
