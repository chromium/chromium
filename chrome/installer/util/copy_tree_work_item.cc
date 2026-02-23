// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/copy_tree_work_item.h"

#include "base/files/file_util.h"
#include "base/logging.h"

CopyTreeWorkItem::~CopyTreeWorkItem() = default;

CopyTreeWorkItem::CopyTreeWorkItem(const base::FilePath& source_path,
                                   const base::FilePath& dest_path,
                                   const base::FilePath& temp_path)
    : source_path_(source_path),
      dest_path_(dest_path),
      temp_path_(temp_path),
      copied_to_dest_path_(false),
      moved_to_backup_(false),
      backup_path_created_(false) {}

bool CopyTreeWorkItem::DoImpl() {
  if (!base::PathExists(source_path_)) {
    LOG(ERROR) << source_path_.value() << " does not exist";
    return false;
  }

  // Move `dest_path` to a backup directory if it exists.
  if (base::PathExists(dest_path_)) {
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
}
