// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the unpack function needed to unpack compressed and
// uncompressed archives in setup.exe.

#include "chrome/installer/setup/unpack_archive.h"

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "chrome/installer/setup/archive_patch_helper.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

namespace {

// Returns nullptr if no compressed archive is available for processing,
// otherwise returns a patch helper configured to uncompress and patch.
std::unique_ptr<ArchivePatchHelper> CreateChromeArchiveHelper(
    const base::FilePath& setup_exe,
    const base::FilePath& install_archive,
    const InstallerState& installer_state,
    const base::FilePath& working_directory,
    UnPackConsumer consumer) {
  // A compressed archive is ordinarily given on the command line by the mini
  // installer. If one was not given, look for chrome.packed.7z next to the
  // running program.
  base::FilePath compressed_archive =
      install_archive.empty()
          ? setup_exe.DirName().Append(kChromeCompressedArchive)
          : install_archive;

  // Fail if no compressed archive is found.
  if (!base::PathExists(compressed_archive)) {
    LOG_IF(ERROR, !install_archive.empty())
        << switches::kInstallArchive << "=" << compressed_archive.value()
        << " not found.";
    return nullptr;
  }

  // chrome.7z is either extracted directly from the compressed archive into the
  // working dir or is the target of patching in the working dir.
  base::FilePath target(working_directory.Append(kChromeArchive));
  DCHECK(!base::PathExists(target));

  // Specify an empty path for the patch source since it isn't yet known that
  // one is needed. It will be supplied in UncompressAndPatchChromeArchive if it
  // is.
  return std::make_unique<ArchivePatchHelper>(
      working_directory, compressed_archive, base::FilePath(), target,
      consumer);
}

}  // namespace

// Workhorse for producing an uncompressed archive (chrome.7z) given a
// chrome.packed.7z containing either a patch file based on the version of
// chrome being updated or the full uncompressed archive. Returns true on
// success, in which case |archive_type| is populated based on what was found.
// Returns false on failure, in which case |install_status| contains the error
// code and the result is written to the registry (via WriteInstallerResult).
base::expected<void, InstallStatus> UncompressAndPatchChromeArchive(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    ArchivePatchHelper* archive_helper,
    ArchiveType* archive_type,
    const base::Version& previous_version) {
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
  //
  // Setup.Install.LzmaUnPackResult_ChromeArchivePatch
  // 0.09% DISK_FULL
  // 0.01% FILE_NOT_FOUND
  //
  // More information can also be found with metrics:
  // Setup.Install.LzmaUnPackNTSTATUS_CompressedChromeArchive
  // Setup.Install.LzmaUnPackNTSTATUS_ChromeArchivePatch
  if (!archive_helper->Uncompress(nullptr)) {
    installer_state.WriteInstallerResult(
        UNCOMPRESSION_FAILED, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, nullptr);
    return base::unexpected(UNCOMPRESSION_FAILED);
  }

  // Short-circuit if uncompression produced the uncompressed archive rather
  // than a patch file.
  if (base::PathExists(archive_helper->target())) {
    *archive_type = FULL_ARCHIVE_TYPE;
    return base::ok();
  }

  // Find the installed version's archive to serve as the source for patching.
  base::FilePath patch_source(
      FindArchiveToPatch(original_state, installer_state, previous_version));
  if (patch_source.empty()) {
    LOG(ERROR) << "Failed to find archive to patch.";
    installer_state.WriteInstallerResult(DIFF_PATCH_SOURCE_MISSING,
                                         IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                                         nullptr);
    return base::unexpected(DIFF_PATCH_SOURCE_MISSING);
  }
  archive_helper->set_patch_source(patch_source);

  // UMA tells us the following about the time required for patching as of M75:
  // --- Foreground ---
  //   12s (50%ile) / 3-6m (99%ile)
  // --- Background ---
  //   1m (50%ile) / >60m (99%ile)
  installer_state.SetStage(PATCHING);
  if (!archive_helper->ApplyAndDeletePatch()) {
    installer_state.WriteInstallerResult(APPLY_DIFF_PATCH_FAILED,
                                         IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                                         nullptr);
    return base::unexpected(APPLY_DIFF_PATCH_FAILED);
  }

  *archive_type = INCREMENTAL_ARCHIVE_TYPE;
  return base::ok();
}

base::expected<void, InstallStatus> UnpackAndMaybePatchChromeArchive(
    const base::FilePath& unpack_path,
    InstallationState& original_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    const InstallerState& installer_state,
    ArchiveType* archive_type,
    base::FilePath& uncompressed_archive) {
  *archive_type = UNKNOWN_ARCHIVE_TYPE;
  base::FilePath install_archive =
      cmd_line.GetSwitchValuePath(switches::kInstallArchive);
  // If this is an uncompressed installation then pass the uncompressed
  // chrome.7z directly, so the chrome.packed.7z unpacking step will be
  // bypassed.
  uncompressed_archive =
      cmd_line.GetSwitchValuePath(switches::kUncompressedArchive);
  if (!install_archive.empty() || uncompressed_archive.empty()) {
    if (!uncompressed_archive.empty()) {
      LOG(ERROR)
          << "A compressed archive and an uncompressed archive were both "
             "provided. This is unsupported. Please provide one archive.";
      return base::unexpected(UNSUPPORTED_OPTION);
    }
    base::Version previous_version;
    if (cmd_line.HasSwitch(switches::kPreviousVersion)) {
      previous_version = base::Version(
          cmd_line.GetSwitchValueASCII(switches::kPreviousVersion));
    }

    std::unique_ptr<ArchivePatchHelper> archive_helper(
        CreateChromeArchiveHelper(
            setup_exe, install_archive, installer_state, unpack_path,
            (previous_version.IsValid()
                 ? UnPackConsumer::CHROME_ARCHIVE_PATCH
                 : UnPackConsumer::COMPRESSED_CHROME_ARCHIVE)));
    if (archive_helper) {
      VLOG(1) << "Installing Chrome from compressed archive "
              << archive_helper->compressed_archive().value();
      RETURN_IF_ERROR(UncompressAndPatchChromeArchive(
          original_state, installer_state, archive_helper.get(), archive_type,
          previous_version));
      uncompressed_archive = archive_helper->target();
      DCHECK(!uncompressed_archive.empty());
    }
  }

  // Check for an uncompressed archive alongside the current executable if one
  // was not given or generated.
  if (uncompressed_archive.empty()) {
    uncompressed_archive = setup_exe.DirName().Append(kChromeArchive);
  }

  if (*archive_type == UNKNOWN_ARCHIVE_TYPE) {
    // An archive was not uncompressed or patched above.
    if (uncompressed_archive.empty() ||
        !base::PathExists(uncompressed_archive)) {
      LOG(ERROR) << "Cannot install Chrome without an uncompressed archive.";
      installer_state.WriteInstallerResult(
          INVALID_ARCHIVE, IDS_INSTALL_INVALID_ARCHIVE_BASE, nullptr);
      return base::unexpected(INVALID_ARCHIVE);
    }
    *archive_type = FULL_ARCHIVE_TYPE;
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
  return base::ok();
}

}  // namespace installer
