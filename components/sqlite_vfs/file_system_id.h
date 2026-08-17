// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_FILE_SYSTEM_ID_H_
#define COMPONENTS_SQLITE_VFS_FILE_SYSTEM_ID_H_

#include <optional>

#include "base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <stdint.h>

#include <array>
#elif BUILDFLAG(IS_POSIX)
#include <sys/types.h>
#endif

namespace base {
class File;
}  // namespace base

namespace sqlite_vfs {

enum class Client;

struct COMPONENT_EXPORT(SQLITE_VFS) FileSystemId {
#if BUILDFLAG(IS_WIN)
  uint64_t volume_serial_number;

  // Either a 128-bit `FILE_ID_128` for ReFS or a 64-bit file index with zeros
  // in the high-order bytes.
  std::array<uint8_t, 16> file_id;
#elif BUILDFLAG(IS_POSIX)
  dev_t dev;
  ino_t ino;
#endif

  friend bool operator==(const FileSystemId&, const FileSystemId&) = default;
};

// Returns a unique identifier for the physical file on disk. Returns
// std::nullopt if the filesystem does not support unique file IDs or returns
// degenerate IDs.
COMPONENT_EXPORT(SQLITE_VFS)
std::optional<FileSystemId> GetFileSystemId(Client client,
                                            const base::File& file);

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_FILE_SYSTEM_ID_H_
