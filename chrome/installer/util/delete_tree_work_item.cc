// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_tree_work_item.h"

#include "base/files/file_util.h"
#include "base/logging.h"

DeleteTreeWorkItem::DeleteTreeWorkItem(const base::FilePath& root_path,
                                       const base::FilePath& temp_path)
    : root_path_(root_path), temp_path_(temp_path) {}

DeleteTreeWorkItem::~DeleteTreeWorkItem() = default;

bool DeleteTreeWorkItem::DoImpl() {
  if (root_path_.empty() || !base::PathExists(root_path_))
    return true;

  // If rollback is not enabled, try to delete the root without making a backup.
  // When that fails, fall back to moving the root to the temporary backup path.
  // Consumers are responsible for making a best-effort attempt to remove the
  // backup path. SelfCleaningTempDir is generally used for the backup path, so
  // in the worst case the file(s) will be removed after the next reboot.
  if (!rollback_enabled() && DeleteRoot())
    return true;

  // Attempt to move the root to the backup.
  return MoveRootToBackup();
}

void DeleteTreeWorkItem::RollbackImpl() {
  if (moved_to_backup_) {
    const base::FilePath& backup = GetBackupPath();
    DCHECK(!backup.empty());
    if (base::PathExists(backup))
      base::Move(backup, root_path_);
  }
}

const base::FilePath& DeleteTreeWorkItem::GetBackupPath() {
  if (backup_path_.empty()) {
    if (!backup_dir_.CreateUniqueTempDirUnderPath(temp_path_)) {
      PLOG(ERROR) << "Failed to get backup path in folder "
                  << temp_path_.value();
      return backup_path_;
    }
    backup_path_ = backup_dir_.GetPath().Append(root_path_.BaseName());
  }

  DCHECK(!backup_path_.empty());
  return backup_path_;
}

bool DeleteTreeWorkItem::DeleteRoot() {
  if (base::DeletePathRecursively(root_path_))
    return true;
  LOG(ERROR) << "Failed to delete " << root_path_.value();
  return false;
}

bool DeleteTreeWorkItem::MoveRootToBackup() {
  const base::FilePath& backup = GetBackupPath();
  if (backup.empty())
    return false;
  if (base::Move(root_path_, backup)) {
    moved_to_backup_ = true;
    return true;
  }
  PLOG(ERROR) << "Failed to move " << root_path_.value() << " to backup path "
              << backup.value();
  return false;
}
