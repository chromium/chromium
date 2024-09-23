// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace ash::network_handler {

COMPONENT_EXPORT(CHROMEOS_NETWORK) extern const char kDBusFailedError[];

// On success, |result| contains the result. On failure, |result| is nullopt.
using ResultCallback =
    base::OnceCallback<void(const std::string& service_path,
                            std::optional<base::Value::Dict> result)>;

// On success, |properties| contains the resulting properties and |error| is
// nullopt. On failure, |result| is nullopt and |error| may contain an error
// identifier.
using PropertiesCallback =
    base::OnceCallback<void(const std::string& service_path,
                            std::optional<base::Value::Dict> properties,
                            std::optional<std::string> error)>;

// An error callback used by both the configuration handler and the state
// handler to receive error results from the API.
using ErrorCallback = base::OnceCallback<void(const std::string& error_name)>;

using ServiceResultCallback =
    base::OnceCallback<void(const std::string& service_path,
                            const std::string& guid)>;

// If not NULL, runs |error_callback| with an ErrorData dictionary created from
// the other arguments.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void RunErrorCallback(ErrorCallback error_callback,
                      const std::string& error_name);

// Callback for Shill errors.
// |error_name| is the error name passed to |error_callback|.
// |path| is the associated object path or blank if not relevant.
// |dbus_error_name| and |dbus_error_message| are provided by the DBus handler.
// Logs an error and calls |error_callback| if not null.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void ShillErrorCallbackFunction(const std::string& error_name,
                                const std::string& path,
                                ErrorCallback error_callback,
                                const std::string& dbus_error_name,
                                const std::string& dbus_error_message);

}  // namespace ash::network_handler

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_HANDLER_CALLBACKS_H_
