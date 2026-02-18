// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_MOVE_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_MOVE_TREE_WORK_ITEM_H_

#include "base/files/file_path.h"
#include "chrome/installer/util/file_conductor.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively move a file system hierarchy from
// source path to destination path. The file system hierarchy could be a
// single file, or a directory.
//
// Under the cover MoveTreeWorkItem moves the destination path, if existing,
// to the temporary directory passed in, and then moves the source hierarchy
// to the destination location. During rollback the original destination
// hierarchy is moved back.
class MoveTreeWorkItem : public WorkItem {
 public:
  ~MoveTreeWorkItem() override;

 private:
  friend class WorkItem;

  // |source_path| specifies file or directory that will be moved to location
  // specified by |dest_path|. To facilitate rollback, the caller needs to
  // supply a temporary directory, |temp_path| to save the original files if
  // they exist under dest_path. If `options.check_for_duplicates` is true, then
  // DoImpl() will first check whether the directory tree in source_path is
  // entirely contained in dest_path and all files in source_path are present
  // and of the same length in dest_path. If so, it will delete `source_path`
  // and return true, otherwise it will attempt to move `source_path` to
  // `dest_path` as stated above. If `options.lenient_deletion` is true, then
  // `DoImpl()` will report success if `dest_path` is properly populated but
  // `source_path` is not entirely removed.
  MoveTreeWorkItem(const base::FilePath& source_path,
                   const base::FilePath& dest_path,
                   const base::FilePath& temp_path,
                   MoveTreeOptions options);

  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Source path to move files from.
  const base::FilePath source_path_;

  // Destination path to move files to.
  const base::FilePath dest_path_;

  installer::FileConductor file_conductor_;

  const MoveTreeOptions options_;
};

#endif  // CHROME_INSTALLER_UTIL_MOVE_TREE_WORK_ITEM_H_
