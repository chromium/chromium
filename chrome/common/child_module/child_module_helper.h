// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_MODULE_CHILD_MODULE_HELPER_H_
#define CHROME_COMMON_CHILD_MODULE_CHILD_MODULE_HELPER_H_

#include "base/files/file_path.h"
#include "base/version.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
namespace base::win {
class Sid;
}  // namespace base::win
#endif

namespace child_module {

// Sub-directory name under the running browser's base version directory where
// updated child module folders are staged.
inline constexpr base::FilePath::CharType kModulesDirName[] =
    FILE_PATH_LITERAL("ChildModules");

// Sentinel file name indicating that a staged version is complete and ready for
// use. This is the component's inner manifest, which the installer copies into
// place only after all other files have been copied.
inline constexpr base::FilePath::CharType kManifestFilename[] =
    FILE_PATH_LITERAL("manifest.json");

// Resolves the default directory where child module versions are staged for the
// currently running browser base version:
//   - Windows: <InstallDir>/<BaseVersion>/ChildModules/
//   - Non-Windows: <DIR_USER_DATA>/ChildModules/<BaseVersion>/
base::FilePath GetModulesDir();

// Resolves the path to the sentinel manifest file directly under `version_dir`.
base::FilePath GetManifestPath(const base::FilePath& version_dir);

// Resolves the path to the renderer binary for `version` under the default
// child modules directory.
base::FilePath GetRendererBinaryPath(const base::Version& version);

#if BUILDFLAG(IS_WIN)
// Derives a distinct path component from `sid` and `canonical_user_data_dir`,
// which must have been produced by the browser's `CanonicalizeUserDataDir()`
// (see chrome/browser/child_module/child_module_paths.h). Returns an empty
// path on error (e.g., if `canonical_user_data_dir` is empty or relative).
// This is a pure function of its inputs that does not access the filesystem.
// Path separators are normalized, trailing separators are removed, and ASCII
// characters are folded to lowercase prior to use.
base::FilePath ComputeUserPathComponent(
    const base::win::Sid& sid,
    const base::FilePath& canonical_user_data_dir);
#endif

}  // namespace child_module

#endif  // CHROME_COMMON_CHILD_MODULE_CHILD_MODULE_HELPER_H_
