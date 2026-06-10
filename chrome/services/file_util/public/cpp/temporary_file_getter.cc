// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/temporary_file_getter.h"

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"

namespace {

constexpr int kMaxNumberOfFilesAllowed = 10;

base::File TemporaryFileGetterHelper(int num_files_requested) {
  base::FilePath temp_path;
  base::File temp_file;
  if (num_files_requested <= kMaxNumberOfFilesAllowed &&
      base::CreateTemporaryFile(&temp_path)) {
    temp_file.Initialize(
        temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                    base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                    base::File::FLAG_DELETE_ON_CLOSE));
  } else {
    temp_file = base::File();
  }
  return temp_file;
}
}  // namespace

TemporaryFileGetter::TemporaryFileGetter() = default;

TemporaryFileGetter::~TemporaryFileGetter() = default;
void TemporaryFileGetter::RequestTemporaryFile(
    RequestTemporaryFileCallback callback) {
  num_files_requested_++;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&TemporaryFileGetterHelper, num_files_requested_),
      std::move(callback));
}
