// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"

namespace breadcrumbs {
namespace {

const base::FilePath::CharType kBreadcrumbsFile[] =
    FILE_PATH_LITERAL("Breadcrumbs");

}  // namespace

base::FilePath GetBreadcrumbPersistentStorageFilePath(
    const base::FilePath& storage_dir) {
  return storage_dir.Append(kBreadcrumbsFile);
}

void DeleteBreadcrumbFiles(const base::FilePath& storage_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::GetDeleteFileCallback(
          breadcrumbs::GetBreadcrumbPersistentStorageFilePath(storage_dir)));
}

}  // namespace breadcrumbs
