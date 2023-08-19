// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/copy_tree_work_item.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/win/shlwapi.h"

CopyTreeWorkItem::~CopyTreeWorkItem() {}

CopyTreeWorkItem::CopyTreeWorkItem(const base::FilePath& source_path,
                                   const base::FilePath& dest_path,
                                   const base::FilePath& temp_path,
                                   CopyOverWriteOption overwrite_option,
                                   const base::FilePath& alternative_path)
    : source_path_(source_path),
      dest_path_(dest_path),
      temp_path_(temp_path),
      overwrite_option_(overwrite_option),
      alternative_path_(alternative_path),
      copied_to_dest_path_(false),
      moved_to_backup_(false),
      copied_to_alternate_path_(false),
      backup_path_created_(false) {}

bool CopyTreeWorkItem::DoImpl() {
  if (!base::PathExists(source_path_)) {
    LOG(ERROR) << source_path_.value() << " does not exist";
    return false;
  }

  bool dest_exist = base::PathExists(dest_path_);
  // handle overwrite_option_ = IF_DIFFERENT case.
  if ((dest_exist) &&
      (overwrite_option_ == WorkItem::IF_DIFFERENT) &&  // only for single file
      (!base::DirectoryExists(source_path_)) &&
      (!base::DirectoryExists(dest_path_)) &&
      (base::ContentsEqual(source_path_, dest_path_))) {
    VLOG(1) << "Source file " << source_path_.value()
            << " and destination file " << dest_path_.value()
            << " are exactly same. Returning true.";
    return true;
  } else if ((dest_exist) &&
             (overwrite_option_ == WorkItem::NEW_NAME_IF_IN_USE) &&
             (!base::DirectoryExists(source_path_)) &&
             (!base::DirectoryExists(dest_path_)) &&
             (IsFileInUse(dest_path_))) {
    // handle overwrite_option_ = NEW_NAME_IF_IN_USE case.
    if (alternative_path_.empty() || base::PathExists(alternative_path_) ||
        !base::CopyFile(source_path_, alternative_path_)) {
      LOG(ERROR) << "failed to copy " << source_path_.value() << " to "
                 << alternative_path_.value();
      return false;
    } else {
      copied_to_alternate_path_ = true;
      VLOG(1) << "Copied source file " << source_path_.value()
              << " to alternative path " << alternative_path_.value();
      return true;
    }
  } else if ((dest_exist) && (overwrite_option_ == WorkItem::IF_NOT_PRESENT)) {
    // handle overwrite_option_ = IF_NOT_PRESENT case.
    return true;
  }

  // In all cases that reach here, move dest to a backup path.
  if (dest_exist) {
    if (!backup_path_.CreateUniqueTempDirUnderPath(temp_path_)) {
      PLOG(ERROR) << "Failed to get backup path in folder "
                  << temp_path_.value();
      return false;
    }
    backup_path_created_ = true;

    base::FilePath backup =
        backup_path_.GetPath().Append(dest_path_.BaseName());
    if (base::Move(dest_path_, backup)) {
      moved_to_backup_ = true;
      VLOG(1) << "Moved destination " << dest_path_.value()
              << " to backup path " << backup.value();
    } else {
      PLOG(ERROR) << "failed moving " << dest_path_.value() << " to "
                  << backup.value();
      return false;
    }
  }

  // In all cases that reach here, copy source to destination.
  if (base::CopyDirectory(source_path_, dest_path_, true)) {
    copied_to_dest_path_ = true;
    VLOG(1) << "Copied source " << source_path_.value() << " to destination "
            << dest_path_.value();
  } else {
    LOG(ERROR) << "failed copy " << source_path_.value() << " to "
               << dest_path_.value();
    return false;
  }

  return true;
}

void CopyTreeWorkItem::RollbackImpl() {
  // Normally the delete operations below should not fail unless some
  // programs like anti-virus are inspecting the files we just copied.
  // If this does happen sometimes, we may consider using Move instead of
  // Delete here. For now we just log the error and continue with the
  // rest of rollback operation.
  if (copied_to_dest_path_ && !base::DeletePathRecursively(dest_path_)) {
    LOG(ERROR) << "Can not delete " << dest_path_.value();
  }
  if (moved_to_backup_) {
    base::FilePath backup(backup_path_.GetPath().Append(dest_path_.BaseName()));
    if (!base::Move(backup, dest_path_)) {
      PLOG(ERROR) << "failed move " << backup.value() << " to "
                  << dest_path_.value();
    }
  }
  if (copied_to_alternate_path_ &&
      !base::DeletePathRecursively(alternative_path_)) {
    LOG(ERROR) << "Can not delete " << alternative_path_.value();
  }
}

// static
bool CopyTreeWorkItem::IsFileInUse(const base::FilePath& path) {
  if (!base::PathExists(path))
    return false;

  // A running executable is open with exclusive write access, so attempting to
  // write to it will fail with a sharing violation. A more precise method would
  // be to open the file with DELETE access and attempt to set the delete
  // disposition on the handle. This would fail if the file was mapped into a
  // process's address space, but succeed otherwise. This seems like overkill,
  // however.
  HANDLE handle =
      ::CreateFile(path.value().c_str(), FILE_WRITE_DATA,
                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                   nullptr, OPEN_EXISTING, 0, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    // By and large, we expect the error to be ERROR_SHARING_VIOLATION if the
    // file is being executed (see above). It may also be something like
    // ERROR_ACCESS_DENIED; e.g., if the file was deleted but open handles to it
    // remain. Consider any failure to open the file to mean that it's in-use
    // and shouldn't be replaced.
    return true;
  }

  CloseHandle(handle);
  return false;
}
