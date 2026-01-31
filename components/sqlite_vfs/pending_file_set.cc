// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/pending_file_set.h"

namespace sqlite_vfs {

PendingFileSet::PendingFileSet() = default;
PendingFileSet::PendingFileSet(PendingFileSet&&) = default;
PendingFileSet& PendingFileSet::operator=(PendingFileSet&&) = default;
PendingFileSet::~PendingFileSet() = default;

}  // namespace sqlite_vfs
