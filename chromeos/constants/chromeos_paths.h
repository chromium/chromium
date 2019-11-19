// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_PATHS_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_PATHS_H_

#include "base/component_export.h"

namespace base {
class FilePath;
}

// This file declares path keys for the chromeos module.  These can be used with
// the PathService to access various special directories and files.

namespace chromeos {

enum {
  PATH_START = 7000,

  FILE_DEFAULT_APP_ORDER,  // Full path to the json file that defines the
                           // default app order.
  FILE_MACHINE_INFO,       // Full path to machine hardware info file.
  FILE_VPD,                // Full path to VPD file.
  FILE_UPTIME,             // Full path to the file via which the kernel
                           // exposes the current device uptime.
  FILE_UPDATE_REBOOT_NEEDED_UPTIME,  // Full path to a file in which Chrome can
                                     // store the uptime at which an update
                                     // became necessary. The file should be
                                     // cleared on boot.
  FILE_STARTUP_CUSTOMIZATION_MANIFEST,     // Path to OEM partner startup
                                           // customization manifest.
  DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS,     // Directory under which a cache of
                                           // force-installed extensions is
                                           // maintained for each device-local
                                           // account.
  DIR_DEVICE_LOCAL_ACCOUNT_EXTERNAL_DATA,  // Directory where external data
                                           // referenced by policies is cached
                                           // for device-local accounts.
  DIR_DEVICE_LOCAL_ACCOUNT_COMPONENT_POLICY,  // Directory where policy for
                                              // components is stored for
                                              // device-local accounts.
                                              // Currently this is used for
                                              // policy for extensions.
  DIR_DEVICE_DISPLAY_PROFILES,       // Destination directory for system display
                                     // profiles downloaded from Quirks Server.
  DIR_DEVICE_EXTENSION_LOCAL_CACHE,  // Directory where extension local cache
                                     // is stored.
  DIR_SIGNIN_PROFILE_COMPONENT_POLICY,  // Directory where policy for components
                                        // is stored for the signin profile.
                                        // Currently this is used for policy for
                                        // extensions.
  DIR_PREINSTALLED_COMPONENTS,          // Directory that contains pre-installed
                                        // components.
  DIR_DEVICE_POLICY_EXTERNAL_DATA,  // Directory where device policy external
                                    // data resources are cached.
  PATH_END
};

// Call once to register the provider for the path keys defined above.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) void RegisterPathProvider();

// Overrides some of the paths listed above so that those files can be used
// when not running on ChromeOS. The stubs files will be relative to
// |stubs_dir|. It is not valid to call this when running on ChromeOS.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
void RegisterStubPathOverrides(const base::FilePath& stubs_dir);

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_PATHS_H_
