// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CREATE_DIR_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_CREATE_DIR_WORK_ITEM_H_

#include <windows.h>

#include "base/files/file_path.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that creates a directory with the specified path.
// It also creates all necessary intermediate paths if they do not exist.
class CreateDirWorkItem : public WorkItem {
 public:
  ~CreateDirWorkItem() override;

 private:
  friend class WorkItem;

  explicit CreateDirWorkItem(const base::FilePath& path);

  // WorkItem:
  bool DoImpl() override;

  // Rollback tries to remove all directories created along the path.
  // If the leaf directory or one of the intermediate directories are not
  // empty, the non-empty directory and its parent directories will not be
  // removed.
  void RollbackImpl() override;

  // Get the top most directory that needs to be created in order to create
  // "path_", and set "top_path_" accordingly. if "path_" already exists,
  // "top_path_" is set to empty string.
  void GetTopDirToCreate();

  // Path of the directory to be created.
  base::FilePath path_;

  // The top most directory that needs to be created.
  base::FilePath top_path_;

  bool rollback_needed_;
};

#endif  // CHROME_INSTALLER_UTIL_CREATE_DIR_WORK_ITEM_H_
