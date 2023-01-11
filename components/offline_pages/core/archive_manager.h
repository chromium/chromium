// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_MANAGER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_MANAGER_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

// Class that manages all archive files for offline pages. They are stored in
// different archive directories based on their lifetime types (persistent or
// temporary).
// All tasks are performed using |task_runner_|.
class ArchiveManager {
 public:
  // Used by metrics collection and clearing storage of temporary pages.
  // - |internal_free_disk_space| is the free space of the volume which
  //   contains temporary and private archive directories.
  // - |external_free_disk_space| is the free space of the volume which contains
  //   public archives directory, in most cases it should be Download directory
  //   in /sdcard/.
  // - |{temporary/private}_archives_size| is the size of the directory. Since
  //   these are inside app directory and are fully controlled, it's unnecessary
  //   to calculate the size by iterating and counting.
  // - |public_archives_size| will enumerate all mhtml files in the public
  //   directory and records the total file size. This will include mhtml files
  //   shared into the public directory through other approaches.
  struct StorageStats {
    int64_t internal_archives_size() const {
      return temporary_archives_size + private_archives_size;
    }
    int64_t internal_free_disk_space;
    int64_t external_free_disk_space;
    int64_t temporary_archives_size;
    int64_t private_archives_size;
    int64_t public_archives_size;
  };

  typedef base::OnceCallback<void(
      const ArchiveManager::StorageStats& storage_stats)>
      StorageStatsCallback;

  ArchiveManager(const base::FilePath& temporary_archives_dir,
                 const base::FilePath& private_archives_dir_,
                 const base::FilePath& public_archives_dir,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ArchiveManager(const ArchiveManager&) = delete;
  ArchiveManager& operator=(const ArchiveManager&) = delete;

  virtual ~ArchiveManager();

  // Creates archives directory if one does not exist yet;
  virtual void EnsureArchivesDirCreated(base::OnceCallback<void()> callback);

  // Gets stats about archive storage, i.e. sizes of all archive directories
  // and free disk spaces.
  virtual void GetStorageStats(StorageStatsCallback callback) const;

  // Gets the archive directories.
  const base::FilePath& GetTemporaryArchivesDir() const;
  const base::FilePath& GetPrivateArchivesDir() const;
  virtual const base::FilePath& GetPublicArchivesDir();

 protected:
  // Used for testing.
  ArchiveManager();

 private:
  // Path under which all of the temporary archives should be stored.
  base::FilePath temporary_archives_dir_;
  // Path under which all of the persistent archives should be saved initially.
  base::FilePath private_archives_dir_;
  // Publically accessible path where archives should be moved once ready.
  base::FilePath public_archives_dir_;
  // Task runner for running file operations.
  // Since the task_runner is a SequencedTaskRunner, it's guaranteed that the
  // second task will start after the first one. This is an important assumption
  // for |ArchiveManager::EnsureArchivesDirCreated|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_MANAGER_H_
