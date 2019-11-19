// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_remover.h"

#include <stdint.h>

#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/chrome_cleaner/mojom/zip_archiver.mojom.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/scoped_disable_wow64_redirection.h"
#include "chrome/chrome_cleaner/os/whitelisted_directory.h"

namespace chrome_cleaner {

namespace {

bool RegisterFileForPostRebootRemoval(const base::FilePath path) {
  // Don't allow directories to be deleted post-reboot. The directory will only
  // be deleted if it is empty, and we can't ensure it will be so don't worry
  // about it.
  if (base::DirectoryExists(path))
    return false;

  // MoveFileEx with MOVEFILE_DELAY_UNTIL_REBOOT flag and null destination
  // registers |file_path| to be deleted on the next system restarts.
  constexpr DWORD flags = MOVEFILE_DELAY_UNTIL_REBOOT;
  return ::MoveFileEx(path.value().c_str(), nullptr, flags) != 0;
}

void DeleteEmptyDirectories(base::FilePath directory) {
  while (
      base::DirectoryExists(directory) && base::IsDirectoryEmpty(directory) &&
      !WhitelistedDirectory::GetInstance()->IsWhitelistedDirectory(directory)) {
    // Empty directories deleted in this cleanup are not logged in the matched
    // folders list for the corresponding UwS, because they are not necessarily
    // matched by any rule by the scanner.
    LOG(INFO) << "Deleting empty directory " << SanitizePath(directory);
    if (!base::DeleteFile(directory, /*recursive=*/false))
      break;
    directory = directory.DirName();
  }
}

// Sanity checks file names to ensure they don't contain ".." or specify a
// drive root.
bool IsSafeNameForDeletion(const base::FilePath& path) {
  // Empty path isn't allowed
  if (path.empty())
    return false;

  const base::string16& path_str = path.value();
  // Disallow anything with "\..\".
  if (path_str.find(L"\\..\\") != base::string16::npos)
    return false;

  // Ensure the path does not specify a drive root: require a character other
  // than \/:. after the last :
  size_t last_colon_pos = path_str.rfind(L':');
  if (last_colon_pos == base::string16::npos)
    return true;
  for (size_t index = last_colon_pos + 1; index < path_str.size(); ++index) {
    wchar_t character = path_str[index];
    if (character != L'\\' && character != L'/' && character != L'.')
      return true;
  }
  return false;
}

void OnArchiveDone(FileRemover::QuarantineResultCallback archival_done_callback,
                   mojom::ZipArchiverResultCode result_code) {
  switch (result_code) {
    // If the archive exists, |path| has already been quarantined.
    case mojom::ZipArchiverResultCode::kSuccess:
    case mojom::ZipArchiverResultCode::kZipFileExists:
      std::move(archival_done_callback).Run(QUARANTINE_STATUS_QUARANTINED);
      return;
    case mojom::ZipArchiverResultCode::kIgnoredSourceFile:
      std::move(archival_done_callback).Run(QUARANTINE_STATUS_SKIPPED);
      return;
    default:
      LOG(ERROR) << "ZipArchiver returned an error code: " << result_code;
      break;
  }
  std::move(archival_done_callback).Run(QUARANTINE_STATUS_ERROR);
}

}  // namespace

FileRemover::FileRemover(scoped_refptr<DigestVerifier> digest_verifier,
                         std::unique_ptr<ZipArchiver> archiver,
                         const LayeredServiceProviderAPI& lsp,
                         base::RepeatingClosure reboot_needed_callback)
    : digest_verifier_(digest_verifier),
      archiver_(std::move(archiver)),
      reboot_needed_callback_(reboot_needed_callback) {
  LSPPathToGUIDs providers;
  GetLayeredServiceProviders(lsp, &providers);
  for (const auto& provider : providers)
    deletion_forbidden_paths_.Insert(provider.first);
  deletion_forbidden_paths_.Insert(
      PreFetchedPaths::GetInstance()->GetExecutablePath());
}

FileRemover::~FileRemover() = default;

void FileRemover::RemoveNow(const base::FilePath& path,
                            DoneCallback callback) const {
  ValidateAndQuarantineFile(
      path, base::BindOnce(&FileRemover::RemoveFile, base::Unretained(this)),
      std::move(callback));
}

void FileRemover::RegisterPostRebootRemoval(const base::FilePath& path,
                                            DoneCallback callback) const {
  ValidateAndQuarantineFile(
      path,
      base::BindOnce(&FileRemover::ScheduleRemoval, base::Unretained(this)),
      std::move(callback));
}

FileRemoverAPI::DeletionValidationStatus FileRemover::CanRemove(
    const base::FilePath& file) const {
  if (!ValidateSandboxFilePath(file))
    return DeletionValidationStatus::UNSAFE;

  // Don't remove remote files. Do this before checking the digest so we don't
  // read them unnecessarily.
  if (base::PathExists(file) && !IsFilePresentLocally(file)) {
    LOG(ERROR) << "Cannot remove remote file " << SanitizePath(file);
    return DeletionValidationStatus::FORBIDDEN;
  }
  // Allow removing of all files if |digest_verifier_| is unavailable.
  // Otherwise, allow removing only files unknown to |digest_verifier_|.
  if (digest_verifier_ && digest_verifier_->IsKnownFile(file)) {
    LOG(ERROR) << "Cannot remove known file " << SanitizePath(file);
    return DeletionValidationStatus::FORBIDDEN;
  }
  if (WhitelistedDirectory::GetInstance()->IsWhitelistedDirectory(file)) {
    // We are logging the path in both sanitized and non-sanitized form since
    // this should never happen unless we are breaking something, in which
    // case we will need precise information.
    LOG(ERROR) << "Cannot remove a CSIDL path: " << file.value()
               << "', sanitized: '" << SanitizePath(file);
    return DeletionValidationStatus::FORBIDDEN;
  }

  if (!IsSafeNameForDeletion(file) || !file.IsAbsolute() ||
      deletion_forbidden_paths_.Contains(file)) {
    return DeletionValidationStatus::FORBIDDEN;
  }

  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;
  if (base::DirectoryExists(file))
    return DeletionValidationStatus::FORBIDDEN;

  return DeletionValidationStatus::ALLOWED;
}

void FileRemover::TryToQuarantine(const base::FilePath& path,
                                  QuarantineResultCallback callback) const {
  // Archiver may not be provided in tests.
  if (archiver_ == nullptr) {
    std::move(callback).Run(QUARANTINE_STATUS_DISABLED);
    return;
  }

  archiver_->Archive(path, base::BindOnce(&OnArchiveDone, std::move(callback)));
}

void FileRemover::RemoveFile(const base::FilePath& path,
                             DoneCallback removal_done_callback,
                             QuarantineStatus quarantine_status) const {
  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();
  removal_status_updater->UpdateQuarantineStatus(path, quarantine_status);
  if (quarantine_status == QUARANTINE_STATUS_ERROR) {
    removal_status_updater->UpdateRemovalStatus(
        path, REMOVAL_STATUS_ERROR_IN_ARCHIVER);
    std::move(removal_done_callback).Run(false);
    return;
  }

  if (!base::DeleteFile(path, /*recursive=*/false)) {
    // If the attempt to delete the file fails, propagate the failure as
    // normal so that the engine knows about it and can try a backup action,
    // but also register the file for post-reboot removal in case the engine
    // doesn't have any effective backup action.
    //
    // A potential downside to this implementation is that if the file is
    // later successfully deleted, we might ask users to reboot when no
    // reboot is needed. See b/66944160 for more details.
    if (RegisterFileForPostRebootRemoval(path)) {
      reboot_needed_callback_.Run();
      removal_status_updater->UpdateRemovalStatus(
          path, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK);
    } else {
      removal_status_updater->UpdateRemovalStatus(
          path, REMOVAL_STATUS_FAILED_TO_REMOVE);
    }
    std::move(removal_done_callback).Run(false);
    return;
  }
  removal_status_updater->UpdateRemovalStatus(path, REMOVAL_STATUS_REMOVED);
  DeleteEmptyDirectories(path.DirName());
  std::move(removal_done_callback).Run(true);
}

void FileRemover::ScheduleRemoval(
    const base::FilePath& file_path,
    FileRemover::DoneCallback removal_done_callback,
    QuarantineStatus quarantine_status) const {
  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();
  removal_status_updater->UpdateQuarantineStatus(file_path, quarantine_status);
  if (quarantine_status == QUARANTINE_STATUS_ERROR) {
    removal_status_updater->UpdateRemovalStatus(
        file_path, REMOVAL_STATUS_ERROR_IN_ARCHIVER);
    std::move(removal_done_callback).Run(false);
    return;
  }

  if (!RegisterFileForPostRebootRemoval(file_path)) {
    PLOG(ERROR) << "Failed to schedule delete file: "
                << chrome_cleaner::SanitizePath(file_path);
    removal_status_updater->UpdateRemovalStatus(
        file_path, REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL);
    std::move(removal_done_callback).Run(false);
    return;
  }
  reboot_needed_callback_.Run();
  removal_status_updater->UpdateRemovalStatus(
      file_path, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
  std::move(removal_done_callback).Run(true);
}

void FileRemover::ValidateAndQuarantineFile(
    const base::FilePath& path,
    FileRemover::RemovalCallback removal_callback,
    FileRemover::DoneCallback done_callback) const {
  DeletionValidationStatus status = CanRemove(path);
  if (status == DeletionValidationStatus::UNSAFE) {
    // Can't record the status of this removal because it's not even safe to
    // normalize the path.
    std::move(done_callback).Run(false);
    return;
  }

  const base::FilePath normalized_path = NormalizePath(path);

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();
  switch (status) {
    case DeletionValidationStatus::UNSAFE:
      // Should be handled above.
      NOTREACHED();
      break;
    case DeletionValidationStatus::FORBIDDEN:
      removal_status_updater->UpdateRemovalStatus(
          normalized_path, REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL);
      std::move(done_callback).Run(false);
      return;
    case DeletionValidationStatus::ALLOWED:
      // No-op. Proceed to removal.
      break;
  }

  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;
  if (!base::PathExists(normalized_path)) {
    removal_status_updater->UpdateRemovalStatus(normalized_path,
                                                REMOVAL_STATUS_NOT_FOUND);
    std::move(done_callback).Run(true);
    return;
  }

  TryToQuarantine(normalized_path,
                  base::BindOnce(std::move(removal_callback), normalized_path,
                                 base::Passed(&done_callback)));
}

}  // namespace chrome_cleaner
