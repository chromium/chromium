// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/file_monitor_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"

namespace download {

namespace {

// Helper function to calculate total file size in a directory, the total
// disk space and free disk space of the volume that contains that directory.
// Returns false if failed to query disk space or total disk space is empty.
bool CalculateDiskUtilization(const base::FilePath& file_dir,
                              int64_t& total_disk_space,
                              int64_t& free_disk_space,
                              int64_t& files_size) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FileEnumerator file_enumerator(file_dir, false /* recursive */,
                                       base::FileEnumerator::FILES);

  int64_t size = 0;
  // Compute the total size of all files in |file_dir|.
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    if (!base::GetFileSize(path, &size)) {
      DVLOG(1) << "File size query failed.";
      return false;
    }
    files_size += size;
  }

  // Disk space of the volume that |file_dir| belongs to.
  total_disk_space = base::SysInfo::AmountOfTotalDiskSpace(file_dir);
  free_disk_space = base::SysInfo::AmountOfFreeDiskSpace(file_dir);
  if (total_disk_space == -1 || free_disk_space == -1) {
    DVLOG(1) << "System disk space query failed.";
    return false;
  }

  if (total_disk_space == 0) {
    DVLOG(1) << "Empty total system disk space.";
    return false;
  }
  return true;
}

// Creates the download directory if it doesn't exist.
bool InitializeAndCreateDownloadDirectory(const base::FilePath& dir_path) {
  // Create the download directory.
  bool success = base::PathExists(dir_path);
  if (!success) {
    base::File::Error error = base::File::Error::FILE_OK;
    success = base::CreateDirectoryAndGetError(dir_path, &error);
    if (!success)
      stats::LogsFileDirectoryCreationError(error);
  }
  // Records disk utilization histograms.
  if (success) {
    int64_t files_size = 0, total_disk_space = 0, free_disk_space = 0;
    if (CalculateDiskUtilization(dir_path, total_disk_space, free_disk_space,
                                 files_size)) {
      stats::LogFileDirDiskUtilization(total_disk_space, free_disk_space,
                                       files_size);
    }
  }

  return success;
}

void GetFilesInDirectory(const base::FilePath& directory,
                         std::set<base::FilePath>& paths_out) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FileEnumerator file_enumerator(directory, false /* recursive */,
                                       base::FileEnumerator::FILES);

  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    paths_out.insert(path);
  }
}

void DeleteFilesOnFileThread(const std::set<base::FilePath>& paths,
                             stats::FileCleanupReason reason) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int num_delete_attempted = 0;
  int num_delete_failed = 0;
  int num_delete_by_external = 0;
  for (const base::FilePath& path : paths) {
    if (!base::PathExists(path)) {
      num_delete_by_external++;
      continue;
    }

    num_delete_attempted++;
    DCHECK(!base::DirectoryExists(path));

    if (!base::DeleteFile(path, false /* recursive */)) {
      num_delete_failed++;
    }
  }

  stats::LogFileCleanupStatus(reason, num_delete_attempted, num_delete_failed,
                              num_delete_by_external);
}

void DeleteUnknownFilesOnFileThread(
    const base::FilePath& directory,
    const std::set<base::FilePath>& download_file_paths) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::set<base::FilePath> files_in_dir;
  GetFilesInDirectory(directory, files_in_dir);

  std::set<base::FilePath> files_to_remove =
      base::STLSetDifference<std::set<base::FilePath>>(files_in_dir,
                                                       download_file_paths);
  DeleteFilesOnFileThread(files_to_remove, stats::FileCleanupReason::UNKNOWN);
}

bool HardRecoverOnFileThread(const base::FilePath& directory) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::set<base::FilePath> files_in_dir;
  GetFilesInDirectory(directory, files_in_dir);
  DeleteFilesOnFileThread(files_in_dir,
                          stats::FileCleanupReason::HARD_RECOVERY);
  return InitializeAndCreateDownloadDirectory(directory);
}

}  // namespace

FileMonitorImpl::FileMonitorImpl(
    const base::FilePath& download_file_dir,
    const scoped_refptr<base::SequencedTaskRunner>& file_thread_task_runner,
    base::TimeDelta file_keep_alive_time)
    : download_file_dir_(download_file_dir),
      file_keep_alive_time_(file_keep_alive_time),
      file_thread_task_runner_(file_thread_task_runner) {}

FileMonitorImpl::~FileMonitorImpl() = default;

void FileMonitorImpl::Initialize(const InitCallback& callback) {
  base::PostTaskAndReplyWithResult(
      file_thread_task_runner_.get(), FROM_HERE,
      base::Bind(&InitializeAndCreateDownloadDirectory, download_file_dir_),
      callback);
}

void FileMonitorImpl::DeleteUnknownFiles(
    const Model::EntryList& known_entries,
    const std::vector<DriverEntry>& known_driver_entries) {
  std::set<base::FilePath> download_file_paths;
  for (Entry* entry : known_entries) {
    download_file_paths.insert(entry->target_file_path);
  }

  for (const DriverEntry& driver_entry : known_driver_entries) {
    download_file_paths.insert(driver_entry.current_file_path);
  }

  file_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeleteUnknownFilesOnFileThread,
                                download_file_dir_, download_file_paths));
}

void FileMonitorImpl::CleanupFilesForCompletedEntries(
    const Model::EntryList& entries,
    const base::Closure& completion_callback) {
  std::set<base::FilePath> files_to_remove;
  for (auto* entry : entries) {
    files_to_remove.insert(entry->target_file_path);

    // TODO(xingliu): Consider logs life time after the file being deleted on
    // the file thread.
    stats::LogFileLifeTime(base::Time::Now() - entry->completion_time,
                           entry->cleanup_attempt_count);
  }

  file_thread_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DeleteFilesOnFileThread, files_to_remove,
                     stats::FileCleanupReason::TIMEOUT),
      completion_callback);
}

void FileMonitorImpl::DeleteFiles(
    const std::set<base::FilePath>& files_to_remove,
    stats::FileCleanupReason reason) {
  file_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteFilesOnFileThread, files_to_remove, reason));
}

void FileMonitorImpl::HardRecover(const InitCallback& callback) {
  base::PostTaskAndReplyWithResult(
      file_thread_task_runner_.get(), FROM_HERE,
      base::Bind(&HardRecoverOnFileThread, download_file_dir_), callback);
}

}  // namespace download
