// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/copy_tree_work_item.h"

CopyTreeWorkItem::~CopyTreeWorkItem() = default;

CopyTreeWorkItem::CopyTreeWorkItem(const base::FilePath& source_path,
                                   const base::FilePath& dest_path,
                                   const base::FilePath& temp_path)
    : source_path_(source_path),
      dest_path_(dest_path),
      file_conductor_(temp_path) {}

bool CopyTreeWorkItem::DoImpl() {
  return file_conductor_.DeleteEntry(dest_path_) &&
         file_conductor_.CopyEntry(source_path_, dest_path_);
}

void CopyTreeWorkItem::RollbackImpl() {
  file_conductor_.Undo();
}
