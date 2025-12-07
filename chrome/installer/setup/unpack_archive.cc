// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the unpack function needed to unpack compressed and
// uncompressed archives in setup.exe.

#include "chrome/installer/setup/unpack_archive.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

namespace {

// Workhorse for producing an uncompressed archive (chrome.7z) given a
// chrome.packed.7z containing the full uncompressed archive. Returns the path
// to the uncompressed archive, or an error. In the error case, the result is
// wrriten to registry (via WriteInstallerResult).
base::expected<base::FilePath, InstallStatus> UncompressChromeArchive(
    const base::FilePath& compressed_archive,
    const base::FilePath& working_directory,
    const InstallerState& installer_state) {
  installer_state.SetStage(UNCOMPRESSING);
  // UMA tells us the following about the time required for uncompression as of
  // M75:
  // --- Foreground (<10%) ---
  //   Full archive: 7.5s (50%ile) / 52s (99%ile)
  //   Archive patch: <2s (50%ile) / 10-20s (99%ile)
  // --- Background (>90%) ---
  //   Full archive: 22s (50%ile) / >3m (99%ile)
  //   Archive patch: ~2s (50%ile) / 1.5m - >3m (99%ile)
  //
  // The top unpack failure result with 28 days aggregation (>=0.01%)
  // Setup.Install.LzmaUnPackResult_CompressedChromeArchive
  // 13.50% DISK_FULL
  // 0.67% ERROR_NO_SYSTEM_RESOURCES
  // 0.12% ERROR_IO_DEVICE
  // 0.05% INVALID_HANDLE
  // 0.01% INVALID_LEVEL
  // 0.01% FILE_NOT_FOUND
  // 0.01% LOCK_VIOLATION
  // 0.01% ACCESS_DENIED
  base::FilePath output_file;
  UnPackStatus unpack_status =
      UnPackArchive(compressed_archive, working_directory, &output_file);
  RecordUnPackMetrics(unpack_status, UnPackConsumer::COMPRESSED_CHROME_ARCHIVE);
  if (unpack_status != UNPACK_NO_ERROR) {
    installer_state.WriteInstallerResult(
        UNCOMPRESSION_FAILED, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, nullptr);
    return base::unexpected(UNCOMPRESSION_FAILED);
  }
  return base::ok(output_file);
}

}  // namespace

base::expected<base::FilePath, InstallStatus> UnpackChromeArchive(
    const base::FilePath& unpack_path,
    InstallationState& original_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    const InstallerState& installer_state) {
  base::FilePath install_archive =
      cmd_line.GetSwitchValuePath(switches::kInstallArchive);
  // If this is an uncompressed installation then pass the uncompressed
  // chrome.7z directly, so the chrome.packed.7z unpacking step will be
  // bypassed.
  base::FilePath uncompressed_archive =
      cmd_line.GetSwitchValuePath(switches::kUncompressedArchive);
  if (!install_archive.empty() || uncompressed_archive.empty()) {
    if (!uncompressed_archive.empty()) {
      LOG(ERROR)
          << "A compressed archive and an uncompressed archive were both "
             "provided. This is unsupported. Please provide one archive.";
      return base::unexpected(UNSUPPORTED_OPTION);
    }

    // A compressed archive is ordinarily given on the command line by the mini
    // installer. If one was not given, look for chrome.packed.7z next to the
    // running program.
    base::FilePath compressed_archive =
        install_archive.empty()
            ? setup_exe.DirName().Append(kChromeCompressedArchive)
            : install_archive;

    // Uncompress if a compressed archive exists.
    if (base::PathExists(compressed_archive)) {
      VLOG(1) << "Installing Chrome from compressed archive "
              << compressed_archive;
      ASSIGN_OR_RETURN(uncompressed_archive,
                       UncompressChromeArchive(compressed_archive, unpack_path,
                                               installer_state));
    } else {
      LOG_IF(ERROR, !install_archive.empty())
          << switches::kInstallArchive << "=" << compressed_archive
          << " not found.";
    }
  }

  // Check for an uncompressed archive alongside the current executable if one
  // was not given or generated.
  if (uncompressed_archive.empty()) {
    uncompressed_archive = setup_exe.DirName().Append(kChromeArchive);
    if (base::PathExists(uncompressed_archive)) {
      LOG(ERROR) << "Cannot install Chrome without an uncompressed archive.";
      installer_state.WriteInstallerResult(
          INVALID_ARCHIVE, IDS_INSTALL_INVALID_ARCHIVE_BASE, nullptr);
      return base::unexpected(INVALID_ARCHIVE);
    }
  }

  // Unpack the uncompressed archive.
  // UMA tells us the following about the time required to unpack as of M75:
  // --- Foreground ---
  //   <2.7s (50%ile) / 45s (99%ile)
  // --- Background ---
  //   ~14s (50%ile) / >3m (99%ile)
  //
  // The top unpack failure result with 28 days aggregation (>=0.01%)
  // Setup.Install.LzmaUnPackResult_UncompressedChromeArchive
  // 0.66% DISK_FULL
  // 0.04% ACCESS_DENIED
  // 0.01% INVALID_HANDLE
  // 0.01% ERROR_NO_SYSTEM_RESOURCES
  // 0.01% PATH_NOT_FOUND
  // 0.01% ERROR_IO_DEVICE
  //
  // More information can also be found with metric:
  // Setup.Install.LzmaUnPackNTSTATUS_UncompressedChromeArchive
  installer_state.SetStage(UNPACKING);
  UnPackStatus unpack_status = UnPackArchive(uncompressed_archive, unpack_path,
                                             /*output_file=*/nullptr);
  RecordUnPackMetrics(unpack_status,
                      UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE);
  if (unpack_status != UNPACK_NO_ERROR) {
    installer_state.WriteInstallerResult(
        UNPACKING_FAILED, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, nullptr);
    return base::unexpected(UNPACKING_FAILED);
  }
  return base::ok(uncompressed_archive);
}

}  // namespace installer
