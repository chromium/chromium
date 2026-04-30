// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_FILE_TYPE_H_
#define COMPONENTS_SQLITE_VFS_FILE_TYPE_H_

#include "build/build_config.h"

namespace sqlite_vfs {

enum class FileType {
  kMainDb,        // A main .db file.
  kTempDb,        // A temporary or in-memory database.
  kTransientDb,   // A database for an ephemeral table.
  kMainJournal,   // A main rollback journal.
  kTempJournal,   // A rollback journal for a temporary database.
  kSubjournal,    // A statement journal file.
  kSuperJournal,  // A super-journal file.
  kWal,           // A WAL-mode journal.
  kWalIndex,      // A WAL-mode shared-memory index.
#if !BUILDFLAG(IS_WIN)
  kWalIndexReadOnly,  // A read-only handle to the WAL-mode shared-memory index.
#endif
};

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_FILE_TYPE_H_
