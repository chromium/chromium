// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_COMPONENT_WIDEVINE_CDM_HINT_FILE_LINUX_H_
#define CHROME_COMMON_MEDIA_COMPONENT_WIDEVINE_CDM_HINT_FILE_LINUX_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "third_party/widevine/cdm/buildflags.h"

#if !BUILDFLAG(ENABLE_WIDEVINE)
#error "This file only applies when Widevine used."
#endif

#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
#error "This file only applies to desktop Linux."
#endif

namespace base {
class FilePath;
}  // namespace base

// The APIs here wrap the component updated Widevine hint file, which lives
// inside the WidevineCdm folder of the user-data-dir, so that the Linux zygote
// process can preload the latest version of Widevine.
//
// The hint file will be a dictionary with one key:
// {
//     "Path": $path_to_WidevineCdm_directory
// }
//
// $path_to_WidevineCdm_directory will point to a directory structure
// containing:
//     LICENSE
//     manifest.json
//     _platform_specific/
//         linux_x64/
//             libwidevinecdm.so
// The actual executable (and directory containing it) will be platform
// specific. There may be additional files as well as the ones listed above.

// Records a new Widevine path into the hint file, replacing the current
// contents if any. |cdm_base_path| is the directory containing the new
// instance. Returns true if the hint file has been successfully updated,
// otherwise false.
bool UpdateWidevineCdmHintFile(const base::FilePath& cdm_base_path)
    WARN_UNUSED_RESULT;

// Returns the latest component updated Widevine CDM directory. If the hint file
// exists and is valid, returns the CDM base_path with the value loaded from the
// file. Otherwise returns empty base::FilePath(). This function does not verify
// that the path returned exists or not.
base::FilePath GetLatestComponentUpdatedWidevineCdmDirectory()
    WARN_UNUSED_RESULT;

#endif  // CHROME_COMMON_MEDIA_COMPONENT_WIDEVINE_CDM_HINT_FILE_LINUX_H_
