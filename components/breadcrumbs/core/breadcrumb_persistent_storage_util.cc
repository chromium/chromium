// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"

namespace breadcrumbs {
namespace {

const base::FilePath::CharType kBreadcrumbsFile[] =
    FILE_PATH_LITERAL("Breadcrumbs");

const base::FilePath::CharType kBreadcrumbsTempFile[] =
    FILE_PATH_LITERAL("Breadcrumbs.temp");

void DoDeleteBreadcrumbFiles(const base::FilePath& storage_dir) {
  base::DeleteFile(
      breadcrumbs::GetBreadcrumbPersistentStorageFilePath(storage_dir));
  base::DeleteFile(
      breadcrumbs::GetBreadcrumbPersistentStorageTempFilePath(storage_dir));
}

}  // namespace

base::FilePath GetBreadcrumbPersistentStorageFilePath(
    const base::FilePath& storage_dir) {
  return storage_dir.Append(kBreadcrumbsFile);
}

base::FilePath GetBreadcrumbPersistentStorageTempFilePath(
    const base::FilePath& storage_dir) {
  return storage_dir.Append(kBreadcrumbsTempFile);
}

void DeleteBreadcrumbFiles(const base::FilePath& storage_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DoDeleteBreadcrumbFiles, storage_dir));
}

}  // namespace breadcrumbs
