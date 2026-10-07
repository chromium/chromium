// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_MODULE_CHILD_MODULE_HELPER_H_
#define CHROME_COMMON_CHILD_MODULE_CHILD_MODULE_HELPER_H_

#include <functional>

#include "base/containers/flat_set.h"
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

#if BUILDFLAG(IS_WIN)
// Returns the canonical form of `user_data_dir` for use with
// `ComputeUserPathComponent()`, or an empty path if it cannot be determined
// (e.g., the directory does not exist or its canonical path is too long). The
// result is an absolute path with junctions and symbolic links resolved and
// with each component of the path in its on-disk case (Windows preserves case
// in file paths but is not case-sensitive).
//
// This accesses the filesystem and must only be called by the browser. The
// browser must pass the result both to `ComputeUserPathComponent()` and to the
// installer so that both compute the same component. The installer must not
// call this, since it may run as a different user (e.g., SYSTEM) for which the
// path may resolve differently or not at all.
base::FilePath CanonicalizeUserDataDir(const base::FilePath& user_data_dir);

// Derives a distinct path component from `sid` and `canonical_user_data_dir`,
// which must have been produced by `CanonicalizeUserDataDir()`. Returns an
// empty path on error (e.g., if `canonical_user_data_dir` is empty or
// relative). This is a pure function of its inputs that does not access the
// filesystem. Path separators are normalized, trailing separators are removed,
// and ASCII characters are folded to lowercase prior to use.
base::FilePath ComputeUserPathComponent(
    const base::win::Sid& sid,
    const base::FilePath& canonical_user_data_dir);
#endif

}  // namespace child_module

#endif  // CHROME_COMMON_CHILD_MODULE_CHILD_MODULE_HELPER_H_
