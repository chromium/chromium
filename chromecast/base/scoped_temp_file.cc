// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/scoped_temp_file.h"
#include "base/check.h"
#include "base/files/file_util.h"

namespace chromecast {

ScopedTempFile::ScopedTempFile() {
  CHECK(base::CreateTemporaryFile(&path_));
}

ScopedTempFile::~ScopedTempFile() {
  if (FileExists()) {
    CHECK(base::DeleteFile(path_));
  }
}

bool ScopedTempFile::FileExists() const {
  return base::PathExists(path_);
}

bool ScopedTempFile::Write(const std::string& str) {
  CHECK(FileExists());
  return base::WriteFile(path_, str);
}

std::string ScopedTempFile::Read() const {
  CHECK(FileExists());
  std::string result;
  CHECK(ReadFileToString(path_, &result));
  return result;
}

}  // namespace chromecast
