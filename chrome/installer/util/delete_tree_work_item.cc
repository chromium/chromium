// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_tree_work_item.h"

DeleteTreeWorkItem::DeleteTreeWorkItem(const base::FilePath& root_path,
                                       const base::FilePath& temp_path)
    : root_path_(root_path), file_conductor_(temp_path) {}

DeleteTreeWorkItem::~DeleteTreeWorkItem() = default;

bool DeleteTreeWorkItem::DoImpl() {
  // The tree is moved into the backup directory unconditionally; even if
  // rollback is not supported.
  return root_path_.empty() || file_conductor_.DeleteEntry(root_path_);
}

void DeleteTreeWorkItem::RollbackImpl() {
  file_conductor_.Undo();
}
