// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_handler_callbacks.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"

namespace {

bool SuppressError(const std::string& dbus_error_message) {
  if (dbus_error_message == "Wake on WiFi not supported")
    return true;
  return false;
}

}  // namespace

namespace ash::network_handler {

// This message is not user-facing, it should only appear in logs.
const char kDBusFailedError[] = "Error.DBusFailed";

void RunErrorCallback(ErrorCallback error_callback,
                      const std::string& error_name) {
  if (error_callback.is_null())
    return;
  std::move(error_callback).Run(error_name);
}

void ShillErrorCallbackFunction(const std::string& error_name,
                                const std::string& path,
                                ErrorCallback error_callback,
                                const std::string& dbus_error_name,
                                const std::string& dbus_error_message) {
  std::string detail = error_name + ": ";
  if (!path.empty())
    detail += path + ": ";
  detail += dbus_error_name;
  if (!dbus_error_message.empty())
    detail += ": " + dbus_error_message;
  device_event_log::LogLevel log_level =
      SuppressError(dbus_error_message) ? device_event_log::LOG_LEVEL_DEBUG
                                        : device_event_log::LOG_LEVEL_ERROR;
  DEVICE_LOG(::device_event_log::LOG_TYPE_NETWORK, log_level) << detail;

  if (error_callback.is_null())
    return;
  std::move(error_callback).Run(error_name);
}

}  // namespace ash::network_handler
