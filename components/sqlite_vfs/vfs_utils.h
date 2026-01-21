// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_VFS_UTILS_H_
#define COMPONENTS_SQLITE_VFS_VFS_UTILS_H_

#include <optional>

#include "base/component_export.h"
#include "components/sqlite_vfs/pending_file_set.h"

namespace base {
class FilePath;
}  // namespace base

namespace sqlite_vfs {

enum class Client;
class SqliteVfsFileSet;

// Creates a new pending file set for a database at the location described by
// `directory` and `base_name`, granting read-write access. If `single
// connection` is true, the database files are locked for exclusive access;
// otherwise, multiple connections are permitted. If `journal_mode_wal` (which
// may only be true for single connections) is true, the database will use
// write-ahead log journaling. Returns no value in case of error (e.g., if the
// file set's files could not be opened or created).
COMPONENT_EXPORT(SQLITE_VFS)
std::optional<PendingFileSet> MakePendingFileSet(
    Client client,
    const base::FilePath& directory,
    const base::FilePath& base_name,
    bool single_connection,
    bool journal_mode_wal);

// Returns a pending file set for a connection to the backend named `base_name`
// within `directory` for the cache named `base_name` and referenced by
// `file_set`. The returned instance is granted read-only access if
// `read_write` is false; otherwise, read/write access.
//
// DANGER: The caller MUST ensure that `directory` and `base_name` are identical
// to those used when `MakePendingFileSet` was called to obtain `file_set` in
// the first place.
//
// On POSIX, obtaining read-only access to a file set that holds read-write
// handles requires reopening the files by path. If the paths provided here do
// not match the original files, the returned `PendingFileSet` will point to
// different files, potentially leading to data corruption or other undefined
// behavior.
COMPONENT_EXPORT(SQLITE_VFS)
std::optional<PendingFileSet> ShareConnection(const base::FilePath& directory,
                                              const base::FilePath& base_name,
                                              const SqliteVfsFileSet& file_set,
                                              bool read_write);

// Returns the base name for a database file, or an empty path if `file` is not
// a database file.
COMPONENT_EXPORT(SQLITE_VFS)
base::FilePath GetBaseName(const base::FilePath& file);

// Deletes all files associated with the database at `directory`/`base_name`.
// Returns the number of bytes freed.
COMPONENT_EXPORT(SQLITE_VFS)
int64_t DeleteFiles(Client client,
                    const base::FilePath& directory,
                    const base::FilePath& base_name);

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_VFS_UTILS_H_
