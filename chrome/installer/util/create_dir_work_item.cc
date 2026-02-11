// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/create_dir_work_item.h"

#include <windows.h>

#include "base/files/file_util.h"
#include "base/logging.h"

CreateDirWorkItem::~CreateDirWorkItem() = default;

CreateDirWorkItem::CreateDirWorkItem(const base::FilePath& path)
    : path_(path), rollback_needed_(false) {}

void CreateDirWorkItem::GetTopDirToCreate() {
  if (base::PathExists(path_)) {
    top_path_ = base::FilePath();
    return;
  }

  base::FilePath parent_dir(path_);
  do {
    top_path_ = parent_dir;
    parent_dir = parent_dir.DirName();
  } while ((parent_dir != top_path_) && !base::PathExists(parent_dir));
  return;
}

bool CreateDirWorkItem::DoImpl() {
  GetTopDirToCreate();
  if (top_path_.empty()) {
    VLOG(1) << "Directory " << path_ << " already exists.";
    return true;
  }

  base::FilePath parent_dir = top_path_.DirName();
  base::FilePath to_create;
  parent_dir.AppendRelativePath(path_, &to_create);

  bool result = base::CreateDirectory(path_);
  if (result) {
    VLOG(1) << "Created directory " << to_create << " in " << parent_dir;
  } else {
    PLOG(ERROR) << "Failed to create directory " << to_create << " in "
                << parent_dir;
    // TODO(crbug.com/483344784): Log extra diagnostics to understand why the
    // above sometimes fails with ERROR_PATH_NOT_FOUND.
    const DWORD parent_attributes =
        ::GetFileAttributes(parent_dir.value().c_str());
    if (parent_attributes == INVALID_FILE_ATTRIBUTES) {
      PLOG(ERROR) << "Failed to get attributes for " << parent_dir;
    } else if ((parent_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      LOG(ERROR) << parent_dir
                 << " is not a directory; attributes = " << parent_attributes;
    } else {
      LOG(ERROR) << parent_dir << " is a directory.";
    }
  }

  rollback_needed_ = true;

  return result;
}

void CreateDirWorkItem::RollbackImpl() {
  if (!rollback_needed_)
    return;

  // Delete all the directories we created to rollback.
  // Note we can not recursively delete top_path_ since we don't want to
  // delete non-empty directory. (We may have created a shared directory).
  // Instead we walk through path_ to top_path_ and delete directories
  // along the way.
  base::FilePath path_to_delete(path_);

  while (1) {
    if (base::PathExists(path_to_delete)) {
      if (!RemoveDirectory(path_to_delete.value().c_str()))
        break;
    }
    if (path_to_delete == top_path_)
      break;
    path_to_delete = path_to_delete.DirName();
  }

  return;
}
