// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/passwords_directory_util_ios.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"

namespace password_manager {

namespace {
// Synchronously deletes passwords directory.
void DeletePasswordsDirectorySync() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath downloads_directory;
  if (GetPasswordsDirectory(&downloads_directory)) {
    // It is assumed that deleting the directory always succeeds.
    base::DeletePathRecursively(downloads_directory);
  }
}
}  // namespace

bool GetPasswordsDirectory(base::FilePath* directory_path) {
  if (!GetTempDir(directory_path)) {
    return false;
  }
  *directory_path = directory_path->Append(FILE_PATH_LITERAL("passwords"));
  return true;
}

void DeletePasswordsDirectory() {
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
                             base::BindOnce(&DeletePasswordsDirectorySync));
}

}  // namespace password_manager
