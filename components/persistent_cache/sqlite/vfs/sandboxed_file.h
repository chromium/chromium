// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_

#include "base/component_export.h"
#include "base/files/file.h"

namespace persistent_cache {

// Represents a file to be exposed to sql::Database via
// SqliteSandboxedVfsDelegate.
class COMPONENT_EXPORT(PERSISTENT_CACHE) SandboxedFile {
 public:
  enum class AccessRights { kReadWrite, kReadOnly };

  SandboxedFile(base::File file, AccessRights access_rights);
  SandboxedFile(SandboxedFile& other) = delete;
  SandboxedFile& operator=(const SandboxedFile& other) = delete;
  SandboxedFile(SandboxedFile&& other);
  SandboxedFile& operator=(SandboxedFile&& other);
  ~SandboxedFile();

  SandboxedFile Copy() const;

  // Duplicates the underlying file without rendering it invalid. Use to share
  // file access to interfaces requiring a regular `base::File`.
  base::File DuplicateUnderlyingFile() const;

  AccessRights access_rights() const { return access_rights_; }

 private:
  base::File underlying_file_;
  AccessRights access_rights_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_SANDBOXED_FILE_H_
