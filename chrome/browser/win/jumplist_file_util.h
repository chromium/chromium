// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_JUMPLIST_FILE_UTIL_H_
#define CHROME_BROWSER_WIN_JUMPLIST_FILE_UTIL_H_

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"

// Maximum number of icon files allowed to delete per jumplist update.
const int kFileDeleteLimit = 30;

// This method is similar to base::DeleteFileRecursive in
// file_util_win.cc with the following differences.
// 1) It has an input parameter |max_file_deleted| to specify the maximum files
//    allowed to delete as well as the maximum attempt failures allowd per run.
// 2) It deletes only the files in |path|. All subdirectories in |path| are
//    untouched but are considered as attempt failures.
void DeleteFiles(const base::FilePath& path,
                 const base::FilePath::StringType& pattern,
                 int max_file_deleted);

// This method is similar to base::DeleteFile in file_util_win.cc
// with the following differences.
// 1) It has an input parameter |max_file_deleted| to specify the maximum files
//    allowed to delete as well as the maximum attempt failures allowd per run.
// 2) It deletes only the files in |path|. All subdirectories in |path| are
//    untouched but are considered as attempt failures.
// 3) |path| won't be removed even if all its contents are deleted successfully.
void DeleteDirectoryContent(const base::FilePath& path, int max_file_deleted);

// This method firstly calls DeleteDirectoryContent() to delete the contents in
// |path|. If |path| is empty after the call, it is removed.
void DeleteDirectory(const base::FilePath& path, int max_file_deleted);

// Returns true if the directory at |path| has more than |max_files| files.
// Sub-directories are not taken into account here.
bool FilesExceedLimitInDir(const base::FilePath& path, int max_files);

// Deletes all files in the directory at |path| but not in set |cached_files|.
void DeleteNonCachedFiles(const base::FilePath& path,
                          const base::flat_set<base::FilePath>& cached_files);

#endif  // CHROME_BROWSER_WIN_JUMPLIST_FILE_UTIL_H_
