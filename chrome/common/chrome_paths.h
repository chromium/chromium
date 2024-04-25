// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_H__
#define CHROME_COMMON_CHROME_PATHS_H__

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

namespace base {
class FilePath;
}

// This file declares path keys for the chrome module.  These can be used with
// the PathService to access various special directories and files.

namespace chrome {

enum {
  PATH_START = 1000,

  DIR_LOGS = PATH_START,  // Directory where logs should be written.
  DIR_USER_DATA,          // Directory where user data can be written.
  DIR_CRASH_METRICS,      // Directory where crash metrics are written.
  DIR_CRASH_DUMPS,        // Directory where crash dumps are written.
  DIR_LOCAL_TRACES,       // Directory where local traces are written.
#if BUILDFLAG(IS_WIN)
  DIR_WATCHER_DATA,       // Directory where the Chrome watcher stores
                          // data.
  DIR_ROAMING_USER_DATA,  // Directory where user data is stored that
                          // needs to be roamed between computers.
#endif
  DIR_RESOURCES,               // Directory containing separate file resources
                               // used by Chrome at runtime.
  DIR_APP_DICTIONARIES,        // Directory where the global dictionaries are.
  DIR_USER_DOCUMENTS,          // Directory for a user's "My Documents".
  DIR_USER_MUSIC,              // Directory for a user's music.
  DIR_USER_PICTURES,           // Directory for a user's pictures.
  DIR_USER_VIDEOS,             // Directory for a user's videos.
  DIR_DEFAULT_DOWNLOADS_SAFE,  // Directory for a user's
                               // "My Documents/Downloads", (Windows) or
                               // "Downloads". (Linux)
  DIR_DEFAULT_DOWNLOADS,       // Directory for a user's downloads.
  DIR_INTERNAL_PLUGINS,        // Directory where internal plugins reside.
  DIR_COMPONENTS,              // Directory where built-in implementations of
                               // component-updated libraries or data reside.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  DIR_POLICY_FILES,  // Directory for system-wide read-only
                     // policy files that allow sys-admins
                     // to set policies for chrome. This directory
                     // contains subdirectories.
#endif
// TODO(crbug.com/40118868): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_CHROMEOS_ASH) ||                              \
    ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
     BUILDFLAG(CHROMIUM_BRANDING)) ||                          \
    BUILDFLAG(IS_MAC)
  DIR_USER_EXTERNAL_EXTENSIONS,  // Directory for per-user external extensions
                                 // on Chrome Mac and Chromium Linux.
                                 // On Chrome OS, this path is used for OEM
                                 // customization. Getting this path does not
                                 // create it.
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  DIR_STANDALONE_EXTERNAL_EXTENSIONS,  // Directory for 'per-extension'
                                       // definition manifest files that
                                       // describe extensions which are to be
                                       // installed when chrome is run.
#endif
  DIR_EXTERNAL_EXTENSIONS,  // Directory where installer places .crx files.

  DIR_DEFAULT_APPS,      // Directory where installer places .crx files
                         // to be installed when chrome is first run.
  FILE_LOCAL_STATE,      // Path and filename to the file in which
                         // machine/installation-specific state is saved.
  FILE_RECORDED_SCRIPT,  // Full path to the script.log file that
                         // contains recorded browser events for
                         // playback.
  DIR_PNACL_BASE,        // Full path to the base dir for PNaCl.
  DIR_PNACL_COMPONENT,   // Full path to the latest PNaCl version
                         // (subdir of DIR_PNACL_BASE).
#if BUILDFLAG(ENABLE_WIDEVINE)
  DIR_BUNDLED_WIDEVINE_CDM,  // Full path to the directory containing the
                             // bundled Widevine CDM.
  DIR_COMPONENT_UPDATED_WIDEVINE_CDM,  // Base directory of the Widevine CDM
                                       // downloaded by the component updater.
  FILE_COMPONENT_WIDEVINE_CDM_HINT,    // A file in a known location that
                                       // points to the component updated
                                       // Widevine CDM.
#endif
  FILE_RESOURCES_PACK,  // Full path to the .pak file containing binary data.
                        // This includes data for internal pages (e.g., html
                        // files and images), unless these resources are
                        // purposefully split into a separate file.
#if BUILDFLAG(IS_CHROMEOS)
  FILE_RESOURCES_FOR_SHARING_PACK,  // Full path to the shared_resources.pak
                                    // tile containing binary data. This
                                    // includes mapping table from lacros
                                    // resource id to ash resource id, and
                                    // fallback resources info consists of
                                    // resources not included in
                                    // ASH_RESOURCES_PACK.
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  FILE_ASH_RESOURCES_PACK,  // Full path to ash resources.pak file.
#endif
  FILE_DEV_UI_RESOURCES_PACK,  // Full path to the .pak file containing
                               // binary data for internal pages (e.g., html
                               // files and images).
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DIR_CHROMEOS_WALLPAPERS,            // Directory where downloaded chromeos
                                      // wallpapers reside.
  DIR_CHROMEOS_WALLPAPER_THUMBNAILS,  // Directory where downloaded chromeos
                                      // wallpaper thumbnails reside.
  DIR_CHROMEOS_CUSTOM_WALLPAPERS,     // Directory where custom wallpapers
                                      // reside.
  DIR_CHROMEOS_CRD_DATA,  // Directory where Chrome Remote Desktop can store
                          // data that must persist a Chrome restart but that
                          // must be cleared on device reboot.

#endif
#if BUILDFLAG(ENABLE_EXTENSIONS) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC))
  DIR_NATIVE_MESSAGING,       // System directory where native messaging host
                              // manifest files are stored.
  DIR_USER_NATIVE_MESSAGING,  // Directory with Native Messaging Hosts
                              // installed per-user.
#endif
#if !BUILDFLAG(IS_ANDROID)
  DIR_GLOBAL_GCM_STORE,  // Directory where the global GCM instance
                         // stores its data.
#endif

  // Valid only in development environment; TODO(darin): move these
  DIR_GEN_TEST_DATA,  // Directory where generated test data resides.
  DIR_TEST_DATA,      // Directory where unit test data resides.
  DIR_TEST_TOOLS,     // Directory where unit test tools reside.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // File containing the location of the updated TPM firmware binary in the file
  // system.
  FILE_CHROME_OS_TPM_FIRMWARE_UPDATE_LOCATION,

  // Flag file indicating SRK ROCA vulnerability status.
  FILE_CHROME_OS_TPM_FIRMWARE_UPDATE_SRK_VULNERABLE_ROCA,

  // Base directory where user cryptohome mount point (named as hash of
  // username) resides.
  DIR_CHROMEOS_HOMEDIR_MOUNT,
#endif                                       // BUILDFLAG(IS_CHROMEOS_ASH)
  DIR_OPTIMIZATION_GUIDE_PREDICTION_MODELS,  // Directory where verified models
                                             // downloaded by the Optimization
                                             // Guide are stored.
  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

// Get or set the invalid user data dir that was originally specified.
void SetInvalidSpecifiedUserDataDir(const base::FilePath& user_data_dir);
const base::FilePath& GetInvalidSpecifiedUserDataDir();

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_H__
