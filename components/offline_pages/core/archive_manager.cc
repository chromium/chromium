// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/archive_manager.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

namespace offline_pages {

namespace {

using StorageStatsCallback =
    base::OnceCallback<void(const ArchiveManager::StorageStats& storage_stats)>;

void EnsureArchivesDirCreatedImpl(const base::FilePath& archives_dir,
                                  bool is_temp) {
  base::File::Error error = base::File::FILE_OK;
  if (!base::DirectoryExists(archives_dir)) {
    if (!base::CreateDirectoryAndGetError(archives_dir, &error)) {
      LOG(ERROR) << "Failed to create offline pages archive directory: "
                 << base::File::ErrorToString(error);
    }
  }
}

void GetStorageStatsImpl(const base::FilePath& temporary_archives_dir,
                         const base::FilePath& private_archives_dir,
                         const base::FilePath& public_archives_dir,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         StorageStatsCallback callback) {
  ArchiveManager::StorageStats storage_stats = {0, 0, 0, 0, 0};

  // Getting the free disk space of the volume that contains the temporary
  // archives directory. This value will be -1 if the directory is invalid.
  // Currently both temporary and private archive directories are in the
  // internal storage.
  storage_stats.internal_free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(temporary_archives_dir);
  storage_stats.external_free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(public_archives_dir);
  if (!temporary_archives_dir.empty()) {
    storage_stats.temporary_archives_size =
        base::ComputeDirectorySize(temporary_archives_dir);
  }
  if (!private_archives_dir.empty()) {
    storage_stats.private_archives_size =
        base::ComputeDirectorySize(private_archives_dir);
  }
  if (!public_archives_dir.empty()) {
    base::FileEnumerator file_enumerator(public_archives_dir, false,
                                         base::FileEnumerator::FILES);
    while (!file_enumerator.Next().empty()) {
#if BUILDFLAG(IS_WIN)
      std::string extension = base::WideToUTF8(
          file_enumerator.GetInfo().GetName().FinalExtension());
#else
      std::string extension =
          file_enumerator.GetInfo().GetName().FinalExtension();
#endif
      if (extension == "mhtml" || extension == "mht") {
        storage_stats.public_archives_size +=
            file_enumerator.GetInfo().GetSize();
      }
    }
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), storage_stats));
}

}  // namespace

// protected and used for testing.
ArchiveManager::ArchiveManager() = default;

ArchiveManager::ArchiveManager(
    const base::FilePath& temporary_archives_dir,
    const base::FilePath& private_archives_dir,
    const base::FilePath& public_archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : temporary_archives_dir_(temporary_archives_dir),
      private_archives_dir_(private_archives_dir),
      public_archives_dir_(public_archives_dir),
      task_runner_(task_runner) {}

ArchiveManager::~ArchiveManager() = default;

void ArchiveManager::EnsureArchivesDirCreated(
    base::OnceCallback<void()> callback) {
  // The callback will only be invoked once both directories are created.
  if (!temporary_archives_dir_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(EnsureArchivesDirCreatedImpl,
                                  temporary_archives_dir_, true /* is_temp */));
  }
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(EnsureArchivesDirCreatedImpl, private_archives_dir_,
                     false /* is_temp */),
      std::move(callback));
}

void ArchiveManager::GetStorageStats(StorageStatsCallback callback) const {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(GetStorageStatsImpl, temporary_archives_dir_,
                     private_archives_dir_, public_archives_dir_,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     std::move(callback)));
}

const base::FilePath& ArchiveManager::GetTemporaryArchivesDir() const {
  return temporary_archives_dir_;
}

const base::FilePath& ArchiveManager::GetPrivateArchivesDir() const {
  return private_archives_dir_;
}

const base::FilePath& ArchiveManager::GetPublicArchivesDir() {
  return public_archives_dir_;
}

}  // namespace offline_pages
