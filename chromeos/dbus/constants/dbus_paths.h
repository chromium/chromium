// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CONSTANTS_DBUS_PATHS_H_
#define CHROMEOS_DBUS_CONSTANTS_DBUS_PATHS_H_

#include "base/component_export.h"

namespace base {
class FilePath;
}

// This file declares path keys for the chromeos/dbus module.  These can be used
// with the PathService to access various special directories and files.

namespace chromeos {
namespace dbus_paths {

enum {
  PATH_START = 7200,

  DIR_USER_POLICY_KEYS,          // Directory where the session_manager stores
                                 // the user policy keys.
  FILE_OWNER_KEY,                // Full path to the owner key file.
  FILE_INSTALL_ATTRIBUTES,       // Full path to the install attributes file.
  FILE_RMAD_SERVICE_EXECUTABLE,  // Path to rmad executable, used to determine
                                 // if RMA flow is supported on the device.
  FILE_RMAD_SERVICE_STATE,       // Path to rmad state file, used to determine
                                 // if the device is currently in RMA.

  PATH_END
};

// Call once to register the provider for the path keys defined above.
COMPONENT_EXPORT(CHROMEOS_DBUS_CONSTANTS) void RegisterPathProvider();

// Overrides some of the paths listed above so that those files can be used
// when not running on ChromeOS. The stubs files will be relative to
// |stubs_dir|. It is not valid to call this when running on ChromeOS.
COMPONENT_EXPORT(CHROMEOS_DBUS_CONSTANTS)
void RegisterStubPathOverrides(const base::FilePath& stubs_dir);

}  // namespace dbus_paths
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CONSTANTS_DBUS_PATHS_H_
