// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H_

#include "base/files/file_path.h"
#include "chrome/installer/util/file_conductor.h"
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
  // Notes on |temp_path|: to facilitate rollback, the caller needs to supply
  // a temporary directory to save the original files if they exist under
  // |dest_path|.
  CopyTreeWorkItem(const base::FilePath& source_path,
                   const base::FilePath& dest_path,
                   const base::FilePath& temp_path);

  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Source path to copy files from.
  base::FilePath source_path_;

  // Destination path to copy files to.
  base::FilePath dest_path_;

  installer::FileConductor file_conductor_;
};

#endif  // CHROME_INSTALLER_UTIL_COPY_TREE_WORK_ITEM_H_
