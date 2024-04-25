// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_COMPONENT_WIDEVINE_CDM_HINT_FILE_LINUX_H_
#define CHROME_COMMON_MEDIA_COMPONENT_WIDEVINE_CDM_HINT_FILE_LINUX_H_

#include <optional>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

#if !BUILDFLAG(ENABLE_WIDEVINE)
#error "This file only applies when Widevine used."
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
#error "This file only applies to desktop Linux and ChromeOS."
#endif

namespace base {
class FilePath;
class Version;
}  // namespace base

// The APIs here wrap the component updated Widevine hint file, which lives
// inside the WidevineCdm folder of the user-data-dir, so that the Linux zygote
// process can preload the latest version of Widevine. This file is updated
// whenever Component Update for the Widevine CDM runs and selects a different
// Widevine CDM that should be used. As the Widevine CDM on Linux can only be
// loaded at Chrome startup, the CDM can't be changed immediately. This file is
// used to help decide which version of the CDM should be loaded the next time
// Chrome starts.
//
// The hint file will be a dictionary with two keys:
// {
//     "Path": $path_to_WidevineCdm_directory
//     "LastBundledVersion": version of the bundled Widevine CDM
// }
//
// $path_to_WidevineCdm_directory will be a directory containing the Widevine
// CDM. It could refer to the bundled CDM or a CDM downloaded by Component
// Update, depending on what version Component Update determines is current.
// It will point to a directory structure containing:
//     LICENSE
//     manifest.json
//     _platform_specific/
//         linux_x64/
//             libwidevinecdm.so
// The actual executable (and directory containing it) will be platform
// specific. There may be additional files as well as the ones listed above.
//
// "LastBundledVersion" will only be set if there was a bundled Widevine CDM
// available when the hint file is updated (by Component Update selecting a new
// Widevine CDM version). It is used to determine if the bundled Widevine CDM
// changed due to a Chrome update next time Chrome starts.

// Records a new Widevine path into the hint file, replacing the current
// contents if any. |cdm_base_path| is the directory containing the new
// instance. |bundled_version| is the version of the current bundled CDM, if
// any. Returns true if the hint file has been successfully updated, otherwise
// false.
[[nodiscard]] bool UpdateWidevineCdmHintFile(
    const base::FilePath& cdm_base_path,
    std::optional<base::Version> bundled_version);

// Returns the directory containing a Widevine CDM selected by Component
// Update the last time it ran. If there are no CDM updates, it may be the
// bundled CDM directory (if it exists). If the hint file exists and is valid,
// returns the CDM base_path with the value loaded from the file. Otherwise
// returns empty base::FilePath(). This function does not verify that the path
// returned exists or not.
[[nodiscard]] base::FilePath GetHintedWidevineCdmDirectory();

// Returns the version of the bundled Widevine CDM that existed when the hint
// file was last updated (by Component Update the last time it ran). May return
// nullopt if there was no bundled Widevine CDM at the time when the hint file
// was updated or is otherwise not set.
std::optional<base::Version> GetBundledVersionDuringLastComponentUpdate();

#endif  // CHROME_COMMON_MEDIA_COMPONENT_WIDEVINE_CDM_HINT_FILE_LINUX_H_
