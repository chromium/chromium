// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_MODULE_CHILD_MODULE_HELPER_H_
#define CHROME_COMMON_CHILD_MODULE_CHILD_MODULE_HELPER_H_

#include <functional>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/version.h"

namespace child_module {

// Sub-directory name under the running browser's base version directory where
// updated child module folders are staged.
inline constexpr base::FilePath::CharType kModulesDirName[] =
    FILE_PATH_LITERAL("ChildModules");

// Sentinel file name indicating that a staged version is complete and ready for
// use.
inline constexpr base::FilePath::CharType kManifestFilename[] =
    FILE_PATH_LITERAL("manifest");

// Set of available child module versions sorted descending (highest first).
using VersionSet = base::flat_set<base::Version, std::greater<>>;

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

}  // namespace child_module

#endif  // CHROME_COMMON_CHILD_MODULE_CHILD_MODULE_HELPER_H_
