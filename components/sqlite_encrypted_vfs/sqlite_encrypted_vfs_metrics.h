// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_ENCRYPTED_VFS_SQLITE_ENCRYPTED_VFS_METRICS_H_
#define COMPONENTS_SQLITE_ENCRYPTED_VFS_SQLITE_ENCRYPTED_VFS_METRICS_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/time/time.h"

namespace sqlite_encrypted_vfs {

// Lifecycle operations on an `EncryptedDatabase`.
//
// LINT.IfChange(EncryptedVfsDatabaseAction)
enum class DatabaseAction {
  kOpen,
  kDelete,
};
// LINT.ThenChange(//components/sqlite_encrypted_vfs/sqlite_encrypted_vfs_metrics.cc:EncryptedVfsDatabaseAction)

// Key management operations performed by `DatabaseKeyManager`.
//
// LINT.IfChange(EncryptedVfsKeyAction)
enum class KeyAction {
  kCreate,
  kLoad,
  kReseal,
  kDelete,
};
// LINT.ThenChange(//components/sqlite_encrypted_vfs/sqlite_encrypted_vfs_metrics.cc:EncryptedVfsKeyAction)

COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
std::string_view DatabaseActionToString(DatabaseAction action);
COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
std::string_view KeyActionToString(KeyAction action);

COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
std::string GetDatabaseActionHistogramName(DatabaseAction action);
COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
std::string GetDatabaseActionTimeHistogramName(DatabaseAction action);

COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
std::string GetKeyActionHistogramName(KeyAction action);
COMPONENT_EXPORT(SQLITE_ENCRYPTED_VFS)
std::string GetKeyActionTimeHistogramName(KeyAction action);

void RecordDatabaseAction(DatabaseAction action,
                          bool success,
                          base::TimeDelta elapsed);
void RecordKeyAction(KeyAction action, bool success, base::TimeDelta elapsed);

}  // namespace sqlite_encrypted_vfs

#endif  // COMPONENTS_SQLITE_ENCRYPTED_VFS_SQLITE_ENCRYPTED_VFS_METRICS_H_
