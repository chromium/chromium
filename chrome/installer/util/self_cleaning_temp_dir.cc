// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/self_cleaning_temp_dir.h"

#include <windows.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"

namespace installer {

// Populates |base_dir| with the topmost directory in the hierarchy of
// |temp_parent_dir| that does not exist.  If |temp_parent_dir| exists,
// |base_dir| is cleared.
// static
void SelfCleaningTempDir::GetTopDirToCreate(
    const base::FilePath& temp_parent_dir,
    base::FilePath* base_dir) {
  DCHECK(base_dir);

  if (base::PathExists(temp_parent_dir)) {
    // Empty base_dir means that we didn't create any extra directories.
    base_dir->clear();
  } else {
    base::FilePath parent_dir(temp_parent_dir);
    do {
      *base_dir = parent_dir;
      parent_dir = parent_dir.DirName();
    } while (parent_dir != *base_dir && !base::PathExists(parent_dir));
    LOG_IF(WARNING, !base::DirectoryExists(parent_dir))
        << "A non-directory is at the base of the path leading to a desired "
           "temp directory location: "
        << parent_dir.value();
  }
}

SelfCleaningTempDir::SelfCleaningTempDir() {}

SelfCleaningTempDir::~SelfCleaningTempDir() {
  if (!path().empty() && !Delete())
    LOG(WARNING) << "Failed to clean temp dir in dtor " << path().value();
}

bool SelfCleaningTempDir::Initialize(const base::FilePath& parent_dir,
                                     const StringType& temp_name) {
  DCHECK(parent_dir.IsAbsolute());
  DCHECK(!temp_name.empty());

  if (!path().empty()) {
    LOG(DFATAL) << "Attempting to re-initialize a SelfSelfCleaningTempDir.";
    return false;
  }

  base::FilePath temp_dir(parent_dir.Append(temp_name));
  base::FilePath base_dir;
  GetTopDirToCreate(parent_dir, &base_dir);

  if (base::CreateDirectory(temp_dir)) {
    base_dir_ = base_dir;
    temp_dir_ = temp_dir;
    return true;
  }

  return false;
}

bool SelfCleaningTempDir::Delete() {
  if (path().empty()) {
    LOG(DFATAL) << "Attempting to Delete an uninitialized SelfCleaningTempDir.";
    return false;
  }

  base::FilePath next_dir(path().DirName());
  bool schedule_deletes = false;

  // First try to recursively delete the leaf directory managed by our
  // base::ScopedTempDir.
  if (!base::DeletePathRecursively(path())) {
    // That failed, so schedule the temp dir and its contents for deletion after
    // reboot.
    LOG(WARNING) << "Failed to delete temporary directory " << path().value()
                 << ". Scheduling for deletion at reboot.";
    schedule_deletes = true;
    if (!ScheduleDirectoryForDeletion(path()))
      return false;  // Entirely unexpected failure (Schedule logs the reason).
  }

  // Now delete or schedule all empty directories up to and including our
  // base_dir_.  Any that can't be deleted are scheduled for deletion at reboot.
  // This is safe since they'll only be deleted in that case if they're empty.
  if (!base_dir_.empty()) {
    do {
      if (!schedule_deletes && !RemoveDirectory(next_dir.value().c_str())) {
        PLOG_IF(WARNING, GetLastError() != ERROR_DIR_NOT_EMPTY)
            << "Error removing directory " << next_dir.value().c_str();
        schedule_deletes = true;
      }
      if (schedule_deletes) {
        // Ignore the return code.  If we fail to schedule, go ahead and add the
        // other parent directories anyway.
        ScheduleFileSystemEntityForDeletion(next_dir);
      }
      if (next_dir == base_dir_)
        break;  // We just processed the topmost directory we created.
      next_dir = next_dir.DirName();
    } while (true);
  }

  base_dir_.clear();
  temp_dir_.clear();

  return true;
}

}  // namespace installer
