// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_COMMON_DBUS_LIBRARY_ERROR_H_
#define CHROMEOS_DBUS_COMMON_DBUS_LIBRARY_ERROR_H_

#include <string>

#include "base/component_export.h"

namespace chromeos {

// A DBusLibraryError represents an error response received from D-Bus.
enum DBusLibraryError {
  kGenericError = -1,  // Catch-all generic error
  kNoReply = -2,       // debugd did not respond before timeout
  kTimeout = -3        // Unspecified D-Bus timeout (e.g. socket error)
};

// Convert the string representation of a D-Bus error into a
// DBusLibraryError value.
COMPONENT_EXPORT(CHROMEOS_DBUS_COMMON)
DBusLibraryError DBusLibraryErrorFromString(
    const std::string& dbus_error_string);

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_COMMON_DBUS_LIBRARY_ERROR_H_
