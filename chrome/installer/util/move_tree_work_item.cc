// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/move_tree_work_item.h"

#include "chrome/installer/util/duplicate_tree_detector.h"

MoveTreeWorkItem::~MoveTreeWorkItem() = default;

MoveTreeWorkItem::MoveTreeWorkItem(const base::FilePath& source_path,
                                   const base::FilePath& dest_path,
                                   const base::FilePath& temp_path,
                                   MoveTreeOptions options)
    : source_path_(source_path),
      dest_path_(dest_path),
      file_conductor_(temp_path),
      options_(options) {}

bool MoveTreeWorkItem::DoImpl() {
  if (options_.check_for_duplicates &&
      installer::IsIdenticalFileHierarchy(source_path_, dest_path_)) {
    // `dest_path_` is identical to `source_path_`, so satisfy the move by
    // simply deleting `source_path_`.
    return file_conductor_.DeleteEntry(source_path_);
  }

  // Otherwise, clear `dest_path_` (by moving it aside) and then put
  // `source_path_` into the desired location.
  return file_conductor_.DeleteEntry(dest_path_) &&
         file_conductor_.MoveEntry(source_path_, dest_path_,
                                   options_.lenient_deletion);
}

void MoveTreeWorkItem::RollbackImpl() {
  file_conductor_.Undo();
}
