// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/scoped_file.h"

#include <string>

#include "base/base_paths_win.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

// static
std::unique_ptr<ScopedFile> ScopedFile::Create(const base::FilePath& dir,
                                               const std::wstring& file_name,
                                               const std::string& contents) {
  base::FilePath file_path = dir.Append(file_name);
  CHECK(base::PathExists(file_path.DirName()));
  CHECK_LE(contents.length(),
           static_cast<size_t>(std::numeric_limits<int>::max()));
  base::WriteFile(file_path, contents.c_str(),
                  static_cast<int>(contents.length()));
  return std::make_unique<ScopedFile>(file_path);
}

ScopedFile::ScopedFile(const base::FilePath& file_path)
    : file_path_(file_path) {}

ScopedFile::~ScopedFile() {
  if (base::PathExists(file_path_))
    PCHECK(base::DeleteFile(file_path_));
}

const base::FilePath& ScopedFile::file_path() {
  return file_path_;
}
