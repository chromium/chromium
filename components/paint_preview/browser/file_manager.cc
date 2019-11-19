// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/file_manager.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/zlib/google/zip.h"

namespace paint_preview {

namespace {

constexpr char kZipExt[] = ".zip";

std::string HashToHex(const GURL& url) {
  uint32_t hash = base::PersistentHash(url.spec());
  return base::HexEncode(&hash, sizeof(uint32_t));
}

}  // namespace

FileManager::FileManager(const base::FilePath& root_directory)
    : root_directory_(root_directory) {}
FileManager::~FileManager() = default;

size_t FileManager::GetSizeOfArtifactsFor(const GURL& url) {
  base::FilePath path;
  StorageType storage_type = GetPathForUrl(url, &path);
  switch (storage_type) {
    case kDirectory: {
      return base::ComputeDirectorySize(
          root_directory_.AppendASCII(HashToHex(url)));
    }
    case kZip: {
      int64_t file_size = 0;
      if (!base::GetFileSize(path, &file_size) || file_size < 0)
        return 0;
      return file_size;
    }
    case kNone:  // fallthrough
    default:
      return 0;
  }
}

bool FileManager::GetCreatedTime(const GURL& url, base::Time* created_time) {
  base::FilePath path;
  StorageType storage_type = GetPathForUrl(url, &path);
  if (storage_type == FileManager::StorageType::kNone)
    return false;
  base::File::Info info;
  if (!base::GetFileInfo(path, &info))
    return false;
  *created_time = info.creation_time;
  return true;
}

bool FileManager::GetLastModifiedTime(const GURL& url,
                                      base::Time* last_modified_time) {
  base::FilePath path;
  StorageType storage_type = GetPathForUrl(url, &path);
  if (storage_type == FileManager::StorageType::kNone)
    return false;
  base::File::Info info;
  if (!base::GetFileInfo(path, &info))
    return false;
  *last_modified_time = info.last_modified;
  return true;
}

bool FileManager::CreateOrGetDirectoryFor(const GURL& url,
                                          base::FilePath* directory) {
  base::FilePath path;
  StorageType storage_type = GetPathForUrl(url, &path);
  switch (storage_type) {
    case kNone: {
      base::FilePath new_path = root_directory_.AppendASCII(HashToHex(url));
      base::File::Error error = base::File::FILE_OK;
      if (base::CreateDirectoryAndGetError(new_path, &error)) {
        *directory = new_path;
        return true;
      }
      DVLOG(1) << "ERROR: failed to create directory: " << path
               << " with error code " << error;
      return false;
    }
    case kDirectory: {
      *directory = path;
      return true;
    }
    case kZip: {
      base::FilePath dst_path = root_directory_.AppendASCII(HashToHex(url));
      base::File::Error error = base::File::FILE_OK;
      if (!base::CreateDirectoryAndGetError(dst_path, &error)) {
        DVLOG(1) << "ERROR: failed to create directory: " << path
                 << " with error code " << error;
        return false;
      }
      if (!zip::Unzip(path, dst_path)) {
        DVLOG(1) << "ERROR: failed to unzip: " << path << " to " << dst_path;
        return false;
      }
      base::DeleteFile(path, true);
      *directory = dst_path;
      return true;
    }
    default:
      return false;
  }
}

bool FileManager::CompressDirectoryFor(const GURL& url) {
  base::FilePath path;
  StorageType storage_type = GetPathForUrl(url, &path);
  switch (storage_type) {
    case kDirectory: {
      // If there are no files in the directory, zip will succeed, but unzip
      // will not. Thus don't compress since there is no point.
      if (!base::ComputeDirectorySize(path))
        return false;
      base::FilePath dst_path = path.AddExtensionASCII(kZipExt);
      if (!zip::Zip(path, dst_path, /* hidden files */ true))
        return false;
      base::DeleteFile(path, true);
      return true;
    }
    case kZip:
      return true;
    case kNone:  // fallthrough
    default:
      return false;
  }
}

void FileManager::DeleteArtifactsFor(const std::vector<GURL>& urls) {
  for (const auto& url : urls) {
    base::FilePath path;
    StorageType storage_type = GetPathForUrl(url, &path);
    if (storage_type == FileManager::StorageType::kNone)
      continue;
    base::DeleteFile(path, true);
  }
}

void FileManager::DeleteAll() {
  base::DeleteFileRecursively(root_directory_);
}

FileManager::StorageType FileManager::GetPathForUrl(const GURL& url,
                                                    base::FilePath* path) {
  base::FilePath directory_path = root_directory_.AppendASCII(HashToHex(url));
  if (base::PathExists(directory_path)) {
    *path = directory_path;
    return kDirectory;
  }
  base::FilePath zip_path = directory_path.AddExtensionASCII(kZipExt);
  if (base::PathExists(zip_path)) {
    *path = zip_path;
    return kZip;
  }
  return kNone;
}

}  // namespace paint_preview
