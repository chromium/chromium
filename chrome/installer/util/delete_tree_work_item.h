// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_

#include "base/files/file_path.h"
#include "chrome/installer/util/file_conductor.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively deletes a file system hierarchy at the
// given root path. The file system hierarchy could be a single file, or a
// directory.
// The file system hierarchy to be deleted can have one or more key files. If
// specified, deletion will be performed only if all key files are not in use.
class DeleteTreeWorkItem : public WorkItem {
 public:
  ~DeleteTreeWorkItem() override;

 private:
  friend class WorkItem;

  // |root_path| will be moved to |temp_path| (rather than copied there and then
  // deleted). For best results in this case, |root_path| and |temp_path|
  // should be on the same volume; otherwise, the move will be simulated
  // by a copy-and-delete operation.
  DeleteTreeWorkItem(const base::FilePath& root_path,
                     const base::FilePath& temp_path);

  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Root path to delete.
  const base::FilePath root_path_;

  installer::FileConductor file_conductor_;
};

#endif  // CHROME_INSTALLER_UTIL_DELETE_TREE_WORK_ITEM_H_
