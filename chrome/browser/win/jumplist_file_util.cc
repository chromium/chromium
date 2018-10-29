// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist_file_util.h"

#include <windows.h>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_restrictions.h"

void DeleteFiles(const base::FilePath& path,
                 const base::FilePath::StringType& pattern,
                 int max_file_deleted) {
  int success_count = 0;
  int failure_count = 0;

  base::FileEnumerator traversal(
      path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES, pattern);

  for (base::FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    // Try to clear the read-only bit if we find it.
    base::FileEnumerator::FileInfo info = traversal.GetInfo();
    if (info.find_data().dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
      SetFileAttributes(
          current.value().c_str(),
          info.find_data().dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
    }

    // JumpListIcons* directories shouldn't have sub-directories. If any of them
    // does for unknown reasons, don't delete them. Instead, increment the
    // failure count.
    if (info.IsDirectory() || !::DeleteFile(current.value().c_str()))
      failure_count++;
    else
      success_count++;

    // The desired max number of files have been deleted, or the desired max
    // number of failures have been hit.
    if (success_count >= max_file_deleted || failure_count >= max_file_deleted)
      break;
  }
}

void DeleteDirectoryContent(const base::FilePath& path, int max_file_deleted) {
  // Iterating over files can be CPU intensive, ensure this is okay on the current thread and
  // intentionally do not mark this scope with a ScopedBlockingCall to avoid a busy thread being
  // considered inactive in the pool.
  base::AssertLongCPUWorkAllowed();

  if (path.empty() || path.value().length() >= MAX_PATH)
    return;

  DWORD attr = GetFileAttributes(path.value().c_str());
  // We're done if we can't find the path.
  if (attr == INVALID_FILE_ATTRIBUTES)
    return;
  // Try to clear the read-only bit if we find it.
  if ((attr & FILE_ATTRIBUTE_READONLY) &&
      !SetFileAttributes(path.value().c_str(),
                         attr & ~FILE_ATTRIBUTE_READONLY)) {
    return;
  }

  // If |path| is a file, simply delete it. However, since JumpListIcons* are
  // directories, hitting the code inside the if-block below is unexpected.
  if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
    ::DeleteFile(path.value().c_str());
    return;
  }

  // If |path| is a directory, delete at most |max_file_deleted| files in it.
  DeleteFiles(path, L"*", max_file_deleted);
}

void DeleteDirectory(const base::FilePath& path, int max_file_deleted) {
  base::AssertLongCPUWorkAllowed();

  // Delete at most |max_file_deleted| files in |path|.
  DeleteDirectoryContent(path, max_file_deleted);

  ::RemoveDirectory(path.value().c_str());
}

bool FilesExceedLimitInDir(const base::FilePath& path, int max_files) {
  int count = 0;
  base::FileEnumerator file_iter(path, false, base::FileEnumerator::FILES);
  while (!file_iter.Next().empty()) {
    if (++count > max_files)
      return true;
  }
  return false;
}

void DeleteNonCachedFiles(const base::FilePath& path,
                          const base::flat_set<base::FilePath>& cached_files) {
  // Iterating over files can be CPU intensive, ensure this is okay on the current thread and
  // intentionally do not mark this scope with a ScopedBlockingCall to avoid a busy thread being
  // considered inactive in the pool.
  base::AssertLongCPUWorkAllowed();

  base::FileEnumerator traversal(path, false, base::FileEnumerator::FILES);

  for (base::FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    if (cached_files.find(current) != cached_files.end())
      continue;

    // Try to clear the read-only bit if we find it.
    base::FileEnumerator::FileInfo info = traversal.GetInfo();
    if (info.find_data().dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
      SetFileAttributes(
          current.value().c_str(),
          info.find_data().dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
    }

    ::DeleteFile(current.value().c_str());
  }
}
