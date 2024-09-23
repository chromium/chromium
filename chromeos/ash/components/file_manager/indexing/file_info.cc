// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/file_info.h"

namespace ash::file_manager {

FileInfo::FileInfo(const GURL& file_url, int64_t size, base::Time last_modified)
    : file_url(file_url), size(size), last_modified(last_modified) {}

FileInfo::FileInfo(const FileInfo& other) = default;

FileInfo::~FileInfo() = default;

FileInfo& FileInfo::operator=(const FileInfo& other) = default;

bool FileInfo::operator==(const FileInfo& other) const {
  bool remote_id_same = false;
  if (remote_id.has_value()) {
    remote_id_same =
        other.remote_id.has_value() && (*other.remote_id == *remote_id);
  } else {
    remote_id_same = !other.remote_id.has_value();
  }
  return file_url.is_valid() && other.file_url.is_valid() &&
         file_url == other.file_url && size == other.size &&
         last_modified == other.last_modified && remote_id_same;
}

std::ostream& operator<<(std::ostream& out, const FileInfo& file_info) {
  out << "FileInfo(file_url=" << file_info.file_url.spec()
      << ", size=" << file_info.size
      << ", last_modified=" << file_info.last_modified
      << ", remote_id=" << file_info.remote_id.value_or("null") << ")";
  return out;
}

}  // namespace ash::file_manager
