// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_path_set.h"

#include <algorithm>
#include <string>

#include "base/strings/string_util.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"

namespace chrome_cleaner {

FilePathSet::FilePathSet() = default;

FilePathSet::FilePathSet(const FilePathSet& file_path_set) = default;

FilePathSet::FilePathSet(std::initializer_list<const wchar_t*> path_list) {
  for (const wchar_t* path : path_list)
    Insert(base::FilePath(path));
}

FilePathSet::~FilePathSet() = default;

FilePathSet& FilePathSet::operator=(const FilePathSet& file_path_set) = default;

bool FilePathSet::Insert(const base::FilePath& file_path) {
  // Make sure to always add long paths to expanded_disk_footprints so they
  // can be properly compared for equality in different places.
  //
  // TODO(joenotcharles): Should return false if |file_path| is relative.
  // Possibly this check should be after NormalizePath so that paths starting
  // with %SystemRoot% are handled.
  return file_paths_.insert(NormalizePath(file_path)).second;
}

bool FilePathSet::Contains(const base::FilePath& file_path) const {
  if (file_paths_.find(file_path) != file_paths_.end())
    return true;
  return file_paths_.find(NormalizePath(file_path)) != file_paths_.end();
}

bool FilePathSet::operator==(const FilePathSet& other) const {
  return file_paths_ == other.file_paths_;
}

void FilePathSet::DiscardNonExistingFiles() {
  UnorderedFilePathSet::iterator iter = file_paths_.begin();
  while (iter != file_paths_.end()) {
    base::FilePath unused;
    if (!TryToExpandPath(*iter, &unused)) {
      iter = file_paths_.erase(iter);
    } else {
      ++iter;
    }
  }
}

const std::vector<base::FilePath> FilePathSet::ReverseSorted() const {
  std::vector<base::FilePath> files_paths(file_paths_.begin(),
                                          file_paths_.end());

  std::sort(files_paths.rbegin(), files_paths.rend());

  return files_paths;
}

const std::vector<base::FilePath> FilePathSet::ToVector() const {
  return std::vector<base::FilePath>(file_paths_.begin(), file_paths_.end());
}

}  // namespace chrome_cleaner
