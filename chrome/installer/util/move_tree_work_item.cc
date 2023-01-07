// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/move_tree_work_item.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/win/shlwapi.h"
#include "chrome/installer/util/duplicate_tree_detector.h"

MoveTreeWorkItem::~MoveTreeWorkItem() {}

MoveTreeWorkItem::MoveTreeWorkItem(const base::FilePath& source_path,
                                   const base::FilePath& dest_path,
                                   const base::FilePath& temp_path,
                                   MoveTreeOption duplicate_option)
    : source_path_(source_path),
      dest_path_(dest_path),
      temp_path_(temp_path),
      moved_to_dest_path_(false),
      moved_to_backup_(false),
      source_moved_to_backup_(false),
      duplicate_option_(duplicate_option) {}

bool MoveTreeWorkItem::DoImpl() {
  if (!base::PathExists(source_path_)) {
    LOG(ERROR) << source_path_.value() << " does not exist";
    return false;
  }

  // If dest_path_ exists, we can do one of two things:
  // 1) If the contents of src_path_are already fully contained in dest_path_
  //    then do nothing and return success. Fully contained means the full
  //    file structure with identical files is contained in dest_path_. For
  //    Chrome, if dest_path_ exists, this is expected to be the common case.
  // 2) If the contents of src_path_ are NOT fully contained in dest_path_, we
  //    attempt to backup dest_path_ and replace it with src_path_. This will
  //    fail if files in dest_path_ are in use.
  if (base::PathExists(dest_path_)) {
    // Generate a backup path that can keep the original files under dest_path_.
    if (!backup_path_.CreateUniqueTempDirUnderPath(temp_path_)) {
      PLOG(ERROR) << "Failed to get backup path in folder "
                  << temp_path_.value();
      return false;
    }
    base::FilePath backup =
        backup_path_.GetPath().Append(dest_path_.BaseName());

    if (duplicate_option_ == CHECK_DUPLICATES) {
      if (installer::IsIdenticalFileHierarchy(source_path_, dest_path_)) {
        // The files we are moving are already present in the destination path.
        // We most likely don't need to do anything. As such, just move the
        // source files to the temp folder as backup.
        if (base::Move(source_path_, backup)) {
          source_moved_to_backup_ = true;
          VLOG(1) << "Moved source " << source_path_.value()
                  << " to backup path " << backup.value();
          return true;
        } else {
          // We failed to move the source tree to the backup path. This is odd
          // but just fall through and attempt the regular behaviour as well.
          PLOG(ERROR) << "Failed to backup source " << source_path_.value()
                      << " to backup path " << backup.value()
                      << " for duplicate trees. Trying regular Move instead.";
        }
      } else {
        VLOG(1) << "Source path " << source_path_.value() << " differs from "
                << dest_path_.value() << ", updating now.";
      }
    }

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

  // Now move source to destination.
  if (base::Move(source_path_, dest_path_)) {
    moved_to_dest_path_ = true;
    VLOG(1) << "Moved source " << source_path_.value() << " to destination "
            << dest_path_.value();
  } else {
    PLOG(ERROR) << "failed move " << source_path_.value() << " to "
                << dest_path_.value();
    return false;
  }

  return true;
}

void MoveTreeWorkItem::RollbackImpl() {
  if (moved_to_dest_path_ && !base::Move(dest_path_, source_path_)) {
    PLOG(ERROR) << "Can not move " << dest_path_.value() << " to "
                << source_path_.value();
  }

  if (moved_to_backup_ || source_moved_to_backup_) {
    base::FilePath backup =
        backup_path_.GetPath().Append(dest_path_.BaseName());

    if (moved_to_backup_ && !base::Move(backup, dest_path_)) {
      PLOG(ERROR) << "failed move " << backup.value() << " to "
                  << dest_path_.value();
    }

    if (source_moved_to_backup_ && !base::Move(backup, source_path_)) {
      PLOG(ERROR) << "Can not restore " << backup.value() << " to "
                  << source_path_.value();
    }
  }
}
