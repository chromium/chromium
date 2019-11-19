// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H_

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/gtest_prod_util.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that recursively copies a file system hierarchy from
// source path to destination path. It also creates all necessary intermediate
// paths of the destination path if they do not exist. The file system
// hierarchy could be a single file, or a directory.
// Under the cover CopyTreeWorkItem moves the destination path, if existing,
// to the temporary directory passed in, and then copies the source hierarchy
// to the destination location. During rollback the original destination
// hierarchy is moved back.
// NOTE: It is a best practice to ensure that the temporary directory is on the
// same volume as the destination path.  If this is not the case, the existing
// destination path is not moved, but rather copied, to the destination path.
// This will result in in-use files being left behind, as well as potentially
// losing ACLs or other metadata in the case of a rollback.
class CopyTreeWorkItem : public WorkItem {
 public:
  ~CopyTreeWorkItem() override;

 private:
  friend class WorkItem;

  // See comments on corresponding member variables for the semantics of
  // arguments.
  // Notes on temp_path: to facilitate rollback, the caller needs to supply
  // a temporary directory to save the original files if they exist under
  // dest_path.
  CopyTreeWorkItem(const base::FilePath& source_path,
                   const base::FilePath& dest_path,
                   const base::FilePath& temp_dir,
                   CopyOverWriteOption overwrite_option,
                   const base::FilePath& alternative_path);

  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Checks if the path specified is in use (and hence can not be deleted)
  static bool IsFileInUse(const base::FilePath& path);

  // Source path to copy files from.
  base::FilePath source_path_;

  // Destination path to copy files to.
  base::FilePath dest_path_;

  // Temporary directory that can be used.
  base::FilePath temp_dir_;

  // Controls the behavior for overwriting.
  CopyOverWriteOption overwrite_option_;

  // If overwrite_option_ = NEW_NAME_IF_IN_USE, this variables stores the path
  // to be used if the file is in use and hence we want to copy it to a
  // different path.
  base::FilePath alternative_path_;

  // Whether the source was copied to dest_path_
  bool copied_to_dest_path_;

  // Whether the original files have been moved to backup path under
  // temporary directory. If true, moving back is needed during rollback.
  bool moved_to_backup_;

  // Whether the source was copied to alternative_path_ because dest_path_
  // existed and was in use. Needed during rollback.
  bool copied_to_alternate_path_;

  // The temporary directory into which the original dest_path_ has been moved.
  base::ScopedTempDir backup_path_;

  // Whether |backup_path_| was created.
  bool backup_path_created_;

  FRIEND_TEST_ALL_PREFIXES(CopyTreeWorkItemTest, CopyFileSameContent);
  FRIEND_TEST_ALL_PREFIXES(CopyTreeWorkItemTest, CopyFileInUse);
  FRIEND_TEST_ALL_PREFIXES(CopyTreeWorkItemTest, CopyFileAndCleanup);
  FRIEND_TEST_ALL_PREFIXES(CopyTreeWorkItemTest, NewNameAndCopyTest);
  FRIEND_TEST_ALL_PREFIXES(CopyTreeWorkItemTest, CopyFileInUseAndCleanup);
};

#endif  // CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H_
