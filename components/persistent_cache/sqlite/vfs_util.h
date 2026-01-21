// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_UTIL_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_UTIL_H_

#include "components/persistent_cache/client.h"
#include "components/sqlite_vfs/client.h"

namespace persistent_cache {

constexpr sqlite_vfs::Client VfsClientFromClient(Client client) {
  switch (client) {
    case Client::kCodeCache:
      return sqlite_vfs::Client::kCodeCache;
    case Client::kShaderCache:
      return sqlite_vfs::Client::kShaderCache;
    case Client::kTest:
      return sqlite_vfs::Client::kTest;
  }
}

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_VFS_UTIL_H_
