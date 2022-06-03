// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_handler_callbacks.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"

namespace {

bool SuppressError(const std::string& dbus_error_message) {
  if (dbus_error_message == "Wake on WiFi not supported")
    return true;
  return false;
}

}  // namespace

namespace chromeos {
namespace network_handler {

// None of these messages are user-facing, they should only appear in logs.
const char kDBusFailedError[] = "Error.DBusFailed";
const char kDBusFailedErrorMessage[] = "DBus call failed.";

// These are names of fields in the error data dictionary for ErrorCallback.
const char kErrorName[] = "errorName";
const char kErrorDetail[] = "errorDetail";
const char kDbusErrorName[] = "dbusErrorName";
const char kDbusErrorMessage[] = "dbusErrorMessage";
const char kPath[] = "path";

void RunErrorCallback(ErrorCallback error_callback,
                      const std::string& path,
                      const std::string& error_name,
                      const std::string& error_detail) {
  if (error_callback.is_null())
    return;
  std::move(error_callback)
      .Run(error_name,
           CreateDBusErrorData(path, error_name, error_detail, "", ""));
}

std::unique_ptr<base::DictionaryValue> CreateDBusErrorData(
    const std::string& path,
    const std::string& error_name,
    const std::string& error_detail,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  auto error_data = std::make_unique<base::DictionaryValue>();
  error_data->SetString(kErrorName, error_name);
  error_data->SetString(kErrorDetail, error_detail);
  error_data->SetString(kDbusErrorName, dbus_error_name);
  error_data->SetString(kDbusErrorMessage, dbus_error_message);
  if (!path.empty())
    error_data->SetString(kPath, path);
  return error_data;
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
  std::move(error_callback)
      .Run(error_name,
           CreateDBusErrorData(path, error_name, detail, dbus_error_name,
                               dbus_error_message));
}

}  // namespace network_handler
}  // namespace chromeos
