// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_ENCRYPTED_VFS_SQLITE_ENCRYPTED_VFS_H_
#define COMPONENTS_SQLITE_ENCRYPTED_VFS_SQLITE_ENCRYPTED_VFS_H_

#include "base/component_export.h"

namespace sqlite_encrypted_vfs {

// SQLite VFS wrapper registered as "sqlite_encrypted_vfs".
//
// This VFS delegates standard OS file operations directly to SQLite's default
// underlying OS VFS (sqlite3_vfs_find(nullptr)), while intercepting xOpen,
// xRead, and xWrite to support transparent database encryption.
class COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS) SqliteEncryptedVfs {
 public:
  static constexpr const char kSqliteVfsName[] = "sqlite_encrypted_vfs";

  // Registers "sqlite_encrypted_vfs" with SQLite if not already registered.
  static void Register();

  // Returns the singleton instance of SqliteEncryptedVfs.
  static SqliteEncryptedVfs* GetInstance();

  SqliteEncryptedVfs();
  ~SqliteEncryptedVfs();

  SqliteEncryptedVfs(const SqliteEncryptedVfs&) = delete;
  SqliteEncryptedVfs& operator=(const SqliteEncryptedVfs&) = delete;
};

}  // namespace sqlite_encrypted_vfs

#endif  // COMPONENTS_SQLITE_ENCRYPTED_VFS_SQLITE_ENCRYPTED_VFS_H_
