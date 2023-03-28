// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/common/dbus_library_error.h"

#include <dbus/dbus-protocol.h>

#include <map>
#include <string>

#include "base/no_destructor.h"

namespace chromeos {

DBusLibraryError DBusLibraryErrorFromString(
    const std::string& dbus_error_string) {
  static const base::NoDestructor<std::map<std::string, DBusLibraryError>>
      error_string_map({
          {DBUS_ERROR_NO_REPLY, DBusLibraryError::kNoReply},
          {DBUS_ERROR_TIMEOUT, DBusLibraryError::kTimeout},
      });

  auto it = error_string_map->find(dbus_error_string);
  return it != error_string_map->end() ? it->second
                                       : DBusLibraryError::kGenericError;
}

}  // namespace chromeos
