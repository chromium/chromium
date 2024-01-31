// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_UNPACK_ARCHIVE_H_
#define CHROME_INSTALLER_SETUP_UNPACK_ARCHIVE_H_

#include "base/types/expected.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace installer {

class InstallationState;
class InstallerState;

// Declares the unpack function needed to unpack compressed and
// uncompressed archives in setup.exe. Uncompress and optionally patch the
// archive if an uncompressed archive was not specified on the command line and
// a compressed archive is found. On success, `uncompressed_archive` will be
// given the full path to the uncompressed archive.
base::expected<void, InstallStatus> UnpackAndMaybePatchChromeArchive(
    const base::FilePath& unpack_path,
    InstallationState& original_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    const InstallerState& installer_state,
    ArchiveType* archive_type,
    base::FilePath& uncompressed_archive);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_UNPACK_ARCHIVE_H_
