// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_FILE_UTIL_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_FILE_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/win/scoped_handle.h"

namespace chrome_cleaner {

// Create an empty file named |name| under |folder|. Return false on failure.
bool CreateFileInFolder(const base::FilePath& folder, const std::wstring& name);

// Create an empty file for path |path|. Return true on success.
bool CreateEmptyFile(const base::FilePath& path);

// Create a file |path| with the content |content|.
void CreateFileWithContent(const base::FilePath& path,
                           const char* content,
                           size_t content_length);

// Creates a file |file_name| in |temp_dir| with the specified |content|.
base::win::ScopedHandle CreateFileWithContent(
    const std::string& content,
    const std::wstring& file_name,
    const base::ScopedTempDir& temp_dir);

// Create a file |path| by writing |count| times the content |content|.
void CreateFileWithRepeatedContent(const base::FilePath& path,
                                   const char* content,
                                   size_t content_length,
                                   size_t count);

// Creates a file with |long_name_path| and return the equivalent
// |short_name_path|. The caller is responsible for deleting the created file.
void CreateFileAndGetShortName(const base::FilePath& long_name_path,
                               base::FilePath* short_name_path);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_FILE_UTIL_H_
