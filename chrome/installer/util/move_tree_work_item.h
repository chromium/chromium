// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_MOVE_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_MOVE_TREE_WORK_ITEM_H_

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/gtest_prod_util.h"
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
  FRIEND_TEST_ALL_PREFIXES(MoveTreeWorkItemTest,
                           MoveDirectoryDestExistsCheckForDuplicatesFull);
  FRIEND_TEST_ALL_PREFIXES(MoveTreeWorkItemTest,
                           MoveDirectoryDestExistsCheckForDuplicatesPartial);

  // |source_path| specifies file or directory that will be moved to location
  // specified by |dest_path|. To facilitate rollback, the caller needs to
  // supply a temporary directory, |temp_path| to save the original files if
  // they exist under dest_path.
  // If |check_duplicates| is CHECK_DUPLICATES, then Do() will first check
  // whether the directory tree in source_path is entirely contained in
  // dest_path and all files in source_path are present and of the same length
  // in dest_path. If so, it will do nothing and return true, otherwise it will
  // attempt to move source_path to dest_path as stated above.
  MoveTreeWorkItem(const base::FilePath& source_path,
                   const base::FilePath& dest_path,
                   const base::FilePath& temp_path,
                   MoveTreeOption duplicate_option);

  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Source path to move files from.
  base::FilePath source_path_;

  // Destination path to move files to.
  base::FilePath dest_path_;

  // Temporary directory to backup dest_path_ (if it already exists).
  base::FilePath temp_path_;

  // The temporary directory into which the original dest_path_ has been moved.
  base::ScopedTempDir backup_path_;

  // Whether the source was moved to dest_path_
  bool moved_to_dest_path_;

  // Whether the original files have been moved to backup path under
  // temporary directory. If true, moving back is needed during rollback.
  bool moved_to_backup_;

  // Whether we moved the source files to the backup path instead just to
  // preserve the behaviour of a Move. This can only become true if
  // duplicate_option_ is CHECK_DUPLICATES.
  bool source_moved_to_backup_;

  // Whether to check for duplicates before moving.
  MoveTreeOption duplicate_option_;
};

#endif  // CHROME_INSTALLER_UTIL_MOVE_TREE_WORK_ITEM_H_
