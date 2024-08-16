// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/sandboxed_vfs_delegate.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"

namespace storage {

SandboxedVfsDelegate::SandboxedVfsDelegate(
    std::unique_ptr<FilesystemProxy> filesystem)
    : filesystem_(std::move(filesystem)) {}

SandboxedVfsDelegate::~SandboxedVfsDelegate() = default;

base::File SandboxedVfsDelegate::OpenFile(const base::FilePath& file_path,
                                          int sqlite_requested_flags) {
  return filesystem_
      ->OpenFile(file_path, base::File::FLAG_OPEN_ALWAYS |
                                base::File::FLAG_READ | base::File::FLAG_WRITE)
      .value_or(base::File());
}

int SandboxedVfsDelegate::DeleteFile(const base::FilePath& file_path,
                                     bool sync_dir) {
  return filesystem_->DeleteFile(file_path) ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

std::optional<sql::SandboxedVfs::PathAccessInfo>
SandboxedVfsDelegate::GetPathAccess(const base::FilePath& file_path) {
  std::optional<FilesystemProxy::PathAccessInfo> info =
      filesystem_->GetPathAccess(file_path);
  if (!info)
    return std::nullopt;

  sql::SandboxedVfs::PathAccessInfo access;
  access.can_read = info->can_read;
  access.can_write = info->can_write;
  return access;
}

}  // namespace storage
